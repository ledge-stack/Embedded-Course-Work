#include "entrance/runtime.h"
#include "entrance/dashboard.h"
#include "auth/runtime.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SESSION_MS 300000
#define BODY_MAX 96

/*
 * This module serves the small page below and its login/status/schedule routes.
 * The PIN creates a short-lived session; schedule changes also need a CSRF token.
 */
static const char *TAG = "entrance_web";
static httpd_handle_t server;
static SemaphoreHandle_t session_mutex;
static char session_token[33], csrf_token[33];
static int64_t session_expiry;

static const char page[] =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CoCIS Entrance</title><style>body{font:16px system-ui;background:#eef2f6;color:#172033;"
    "max-width:600px;margin:2rem "
    "auto;padding:1rem}main{background:white;padding:1.5rem;border-radius:12px}"
    "input,button{font:inherit;padding:.65rem;margin:.3rem}button{background:#125c51;color:white;"
    "border:0}"
    ".row{display:flex;flex-wrap:wrap}.status{font-size:1.5rem;font-weight:700}</style></"
    "head><body><main>"
    "<h1>CoCIS Entrance</h1><div id='login'><p>Authorized personnel sign in to view status or "
    "change hours.</p>"
    "<input id='pin' type='password' inputmode='numeric' maxlength='4' placeholder='Admin PIN'>"
    "<button onclick='login()'>Sign in</button></div><div id='app' hidden><p class='status' "
    "id='state'>Loading</p>"
    "<p id='details'></p><form onsubmit='save(event)'><h2>Access hours</h2><div class='row'>"
    "<label>Opens <input id='start' type='time' required></label>"
    "<label>Closes <input id='end' type='time' required></label></div>"
    "<button>Save schedule</button><button type='button' onclick='logout()'>Sign "
    "out</button></form>"
    "<section><h2>People status</h2><ul id='people'></ul></section>"
    "<section><h2>Recent entry and exit</h2><ul id='activity'></ul></section>"
    "<p>Recent activity is from this simulation run. The SD card keeps the longer event record.</p>"
    "<p id='message'></p></div></main><script>let csrf='';"
    "async function login(){let p=document.querySelector('#pin').value;"
    "let r=await "
    "fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/"
    "x-www-form-urlencoded'},"
    "body:'pin='+encodeURIComponent(p)});document.querySelector('#pin').value='';"
    "if(!r.ok){alert('Sign-in failed or temporarily "
    "locked');return}document.querySelector('#login').hidden=true;"
    "document.querySelector('#app').hidden=false;await refresh()}"
    "async function refresh(){let r=await "
    "fetch('/api/status');if(r.status===401){document.querySelector('#login').hidden=false;"
    "document.querySelector('#app').hidden=true;return}let s=await r.json();csrf=s.csrf;"
    "document.querySelector('#state').textContent=s.clock?(s.open?'CoCIS OPEN':'CoCIS "
    "CLOSED'):'Waiting for clock';"
    "document.querySelector('#details').textContent='Opens '+s.start+' · Closes '+s.end+' · Inside "
    "'+s.inside+"
    "' · Last: '+s.last+' '+s.action+' at '+s.at;document.querySelector('#start').value=s.start;"
    "document.querySelector('#end').value=s.end;"
    "let people=document.querySelector('#people');people.replaceChildren();"
    "for(let p of s.people){let li=document.createElement('li');"
    "li.textContent=p.name+' — '+p.state;people.appendChild(li)}"
    "let activity=document.querySelector('#activity');activity.replaceChildren();"
    "for(let e of s.recent){let li=document.createElement('li');"
    "let action=e.granted?(e.entering?'Welcome ':'Goodbye '):'Denied: ';"
    "li.textContent=action+e.name+' ('+(e.entering?'entry':'exit')+' '+e.time+')';"
    "activity.appendChild(li)}}"
    "async function save(e){e.preventDefault();let "
    "a=document.querySelector('#start').value,b=document.querySelector('#end').value;"
    "let r=await "
    "fetch('/api/schedule',{method:'POST',headers:{'Content-Type':'application/"
    "x-www-form-urlencoded',"
    "'X-CSRF-Token':csrf},body:'start='+encodeURIComponent(a)+'&end='+encodeURIComponent(b)});"
    "document.querySelector('#message').textContent=r.ok?'Schedule saved':'Could not save "
    "schedule';await refresh()}"
    "async function logout(){await "
    "fetch('/api/logout',{method:'POST',headers:{'X-CSRF-Token':csrf}});"
    "document.querySelector('#app').hidden=true;document.querySelector('#login').hidden=false}"
    "setInterval(()=>{if(!document.querySelector('#app').hidden)refresh()},3000);</script></body></"
    "html>";

static void make_token(char out[33]) {
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    for (unsigned i = 0; i < sizeof(bytes); i++)
        snprintf(out + i * 2, 3, "%02x", bytes[i]);
    out[32] = 0;
}
static bool token_equal(const char *a, const char *b) {
    if (!a || !b || strlen(a) != 32 || strlen(b) != 32)
        return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < 32; i++)
        diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}
static bool request_session(httpd_req_t *req, bool check_csrf) {
    char cookie[128] = {0}, header[64] = {0};
    size_t n = httpd_req_get_hdr_value_len(req, "Cookie");
    if (!n || n >= sizeof(cookie) ||
        httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK)
        return false;
    char *value = strstr(cookie, "entrance_session=");
    if (!value)
        return false;
    value += strlen("entrance_session=");
    char *end = strchr(value, ';');
    if (end)
        *end = 0;
    /* The mutex keeps token checks and logout from changing the session together. */
    bool valid;
    xSemaphoreTake(session_mutex, portMAX_DELAY);
    valid = esp_timer_get_time() / 1000 < session_expiry && token_equal(value, session_token);
    if (valid)
        session_expiry = esp_timer_get_time() / 1000 + SESSION_MS;
    if (valid && check_csrf) {
        if (httpd_req_get_hdr_value_len(req, "X-CSRF-Token") >= sizeof(header) ||
            httpd_req_get_hdr_value_str(req, "X-CSRF-Token", header, sizeof(header)) != ESP_OK)
            valid = false;
        else
            valid = token_equal(header, csrf_token);
    }
    xSemaphoreGive(session_mutex);
    return valid;
}
static esp_err_t send_error(httpd_req_t *req, int status, const char *message) {
    httpd_resp_set_status(req, status == 401   ? "401 Unauthorized"
                               : status == 403 ? "403 Forbidden"
                                               : "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, message);
}
static bool receive_body(httpd_req_t *req, char *body, size_t capacity) {
    if (!req || !body || req->content_len >= capacity)
        return false;
    size_t received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, body + received, req->content_len - received);
        if (n <= 0)
            return false;
        received += (size_t)n;
    }
    body[received] = 0;
    return true;
}
static int hex_digit(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}
static bool decode_form_body(char *body) {
    char *read = body;
    char *write = body;
    while (*read) {
        if (*read == '%') {
            if (!read[1] || !read[2])
                return false;
            int high = hex_digit(read[1]);
            int low = hex_digit(read[2]);
            if (high < 0 || low < 0)
                return false;
            char decoded = (char)((high << 4) | low);
            if (!decoded)
                return false;
            *write++ = decoded;
            read += 3;
        } else if (*read == '+') {
            *write++ = ' ';
            read++;
        } else {
            *write++ = *read++;
        }
    }
    *write = 0;
    return true;
}
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t login_handler(httpd_req_t *req) {
    if (req->content_len <= 4 || req->content_len > BODY_MAX)
        return send_error(req, 400, "Invalid login");
    char body[BODY_MAX + 1] = {0};
    if (!receive_body(req, body, sizeof(body)))
        return send_error(req, 400, "Invalid login");
    char *pin = strstr(body, "pin=");
    if (!pin)
        return send_error(req, 400, "Invalid login");
    pin += 4;
    char *amp = strchr(pin, '&');
    if (amp)
        *amp = 0;
    bool accepted = auth_runtime_verify_pin(pin);
    memset(body, 0, sizeof(body));
    if (!accepted)
        return send_error(req, 401, "Sign-in failed");
    xSemaphoreTake(session_mutex, portMAX_DELAY);
    make_token(session_token);
    make_token(csrf_token);
    session_expiry = esp_timer_get_time() / 1000 + SESSION_MS;
    char cookie[96];
    snprintf(cookie, sizeof(cookie), "entrance_session=%s; HttpOnly; SameSite=Strict; Path=/",
             session_token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    xSemaphoreGive(session_mutex);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, "OK");
}
static void json_escape(char *out, size_t cap, const char *input) {
    size_t used = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p && used + 2 < cap; p++) {
        if (*p == '"' || *p == '\\')
            out[used++] = '\\';
        if (*p >= 32)
            out[used++] = (char)*p;
    }
    out[used] = 0;
}
static bool append_format(char *out, size_t capacity, size_t *used, const char *format, ...) {
    if (!out || !used || *used >= capacity)
        return false;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(out + *used, capacity - *used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *used)
        return false;
    *used += (size_t)written;
    return true;
}
static void format_time(uint16_t minute, char out[6]) {
    minute = (uint16_t)(minute % 1440u);
    unsigned hour = minute / 60u, mins = minute % 60u;
    out[0] = (char)('0' + hour / 10u);
    out[1] = (char)('0' + hour % 10u);
    out[2] = ':';
    out[3] = (char)('0' + mins / 10u);
    out[4] = (char)('0' + mins % 10u);
    out[5] = 0;
}
static esp_err_t status_handler(httpd_req_t *req) {
    if (!request_session(req, false))
        return send_error(req, 401, "Sign in required");
    entrance_status s;
    if (!entrance_runtime_status(&s))
        return send_error(req, 400, "Status unavailable");
    system_user users[SYSTEM_MAX_USERS];
    bool inside[SYSTEM_MAX_USERS];
    size_t user_count, inside_count;
    if (!entrance_runtime_snapshot(users, inside, SYSTEM_MAX_USERS, &user_count, &inside_count,
                                   NULL))
        return send_error(req, 400, "People status unavailable");
    entrance_access_record recent[ENTRANCE_RECENT_ACCESS_COUNT];
    size_t recent_count;
    if (!entrance_runtime_recent_access(recent, ENTRANCE_RECENT_ACCESS_COUNT, &recent_count))
        return send_error(req, 400, "Recent activity unavailable");
    char last[2 * SYSTEM_NAME_MAX_BYTES + 1];
    json_escape(last, sizeof(last), s.last_person);
    char start[6], end[6];
    format_time(s.schedule.start_minute, start);
    format_time(s.schedule.end_minute, end);
    char response[4096];
    size_t used = 0;
    xSemaphoreTake(session_mutex, portMAX_DELAY);
    bool fits = append_format(
        response, sizeof(response), &used,
        "{\"clock\":%s,\"open\":%s,\"start\":\"%s\",\"end\":\"%s\","
        "\"inside\":%u,\"last\":\"%s\",\"action\":\"%s\",\"at\":\"%s\","
        "\"csrf\":\"%s\",\"people\":[",
        s.clock_valid ? "true" : "false", s.is_open ? "true" : "false", start, end,
        (unsigned)inside_count, last, s.has_last_event ? s.last_action : "NONE",
        s.has_last_event ? s.last_time : "--:--", csrf_token);
    xSemaphoreGive(session_mutex);
    bool first = true;
    for (size_t i = 0; fits && i < SYSTEM_MAX_USERS; i++) {
        if (!users[i].enabled)
            continue;
        char name[2 * SYSTEM_NAME_MAX_BYTES + 1];
        json_escape(name, sizeof(name), users[i].display_name);
        const char *state = inside[i] ? "INSIDE" : s.occupancy_uncertain ? "CHECK" : "OUTSIDE";
        fits = append_format(response, sizeof(response), &used,
                             "%s{\"name\":\"%s\",\"state\":\"%s\"}",
                             first ? "" : ",", name, state);
        first = false;
    }
    fits = fits && append_format(response, sizeof(response), &used, "],\"recent\":[");
    for (size_t i = 0; fits && i < recent_count; i++) {
        char name[2 * SYSTEM_NAME_MAX_BYTES + 1];
        json_escape(name, sizeof(name), recent[i].name);
        fits = append_format(response, sizeof(response), &used,
                             "%s{\"name\":\"%s\",\"entering\":%s,\"granted\":%s,"
                             "\"time\":\"%s\"}",
                             i ? "," : "", name, recent[i].entering ? "true" : "false",
                             recent[i].granted ? "true" : "false", recent[i].time);
    }
    fits = fits && append_format(response, sizeof(response), &used, "]}");
    if (!fits)
        return send_error(req, 400, "Dashboard status is too large");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}
static bool parse_time_value(const char *text, uint16_t *minute) {
    if (!text || strlen(text) != 5 || text[2] != ':' || text[0] < '0' || text[0] > '2' ||
        text[1] < '0' || text[1] > '9' || text[3] < '0' || text[3] > '5' || text[4] < '0' ||
        text[4] > '9')
        return false;
    unsigned h = (unsigned)(text[0] - '0') * 10u + (unsigned)(text[1] - '0');
    unsigned m = (unsigned)(text[3] - '0') * 10u + (unsigned)(text[4] - '0');
    if (h > 23 || m > 59)
        return false;
    *minute = (uint16_t)(h * 60u + m);
    return true;
}
static esp_err_t schedule_handler(httpd_req_t *req) {
    if (!request_session(req, true))
        return send_error(req, 403, "Authorized session and CSRF token required");
    if (req->content_len <= 10 || req->content_len > BODY_MAX)
        return send_error(req, 400, "Invalid schedule");
    char body[BODY_MAX + 1] = {0};
    if (!receive_body(req, body, sizeof(body)) || !decode_form_body(body))
        return send_error(req, 400, "Invalid schedule");
    char *start = strstr(body, "start="), *end = strstr(body, "end=");
    if (!start || !end)
        return send_error(req, 400, "Invalid schedule");
    start += 6;
    char *amp = strchr(start, '&');
    if (amp)
        *amp = 0;
    end += 4;
    uint16_t start_minute, end_minute;
    if (!parse_time_value(start, &start_minute) || !parse_time_value(end, &end_minute))
        return send_error(req, 400, "Use HH:MM");
    esp_err_t e = entrance_runtime_set_schedule((system_access_schedule){start_minute, end_minute});
    if (e != ESP_OK)
        return send_error(req, 400, "Schedule was not saved");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"saved\":true}");
}
static esp_err_t logout_handler(httpd_req_t *req) {
    if (!request_session(req, true))
        return send_error(req, 403, "Session required");
    xSemaphoreTake(session_mutex, portMAX_DELAY);
    memset(session_token, 0, sizeof(session_token));
    memset(csrf_token, 0, sizeof(csrf_token));
    session_expiry = 0;
    xSemaphoreGive(session_mutex);
    httpd_resp_set_hdr(req, "Set-Cookie",
                       "entrance_session=; Max-Age=0; HttpOnly; SameSite=Strict; Path=/");
    return httpd_resp_sendstr(req, "OK");
}

esp_err_t entrance_dashboard_start(void) {
    if (server)
        return ESP_ERR_INVALID_STATE;
    session_mutex = xSemaphoreCreateMutex();
    if (!session_mutex)
        return ESP_ERR_NO_MEM;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    esp_err_t e = httpd_start(&server, &config);
    if (e != ESP_OK)
        return e;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler},
        {.uri = "/api/login", .method = HTTP_POST, .handler = login_handler},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/api/schedule", .method = HTTP_POST, .handler = schedule_handler},
        {.uri = "/api/logout", .method = HTTP_POST, .handler = logout_handler}};
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        e = httpd_register_uri_handler(server, &routes[i]);
        if (e != ESP_OK)
            return e;
    }
    ESP_LOGI(TAG, "authorized entrance dashboard listening on HTTP port 80; Wokwi browser access "
                  "needs the Private IoT Gateway");
    return ESP_OK;
}
