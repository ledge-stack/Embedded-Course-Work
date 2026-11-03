#include "shim.h"
#include <string.h>
typedef struct {
    CRITICAL_SECTION lock;
    HANDLE items, spaces;
    unsigned head, tail, size, length;
    uint8_t *data;
} queue;
QueueHandle_t xQueueCreate(unsigned length, unsigned size) {
    queue *q = calloc(1, sizeof(*q));
    if (!q)
        return NULL;
    q->data = calloc(length, size);
    q->size = size;
    q->length = length;
    InitializeCriticalSection(&q->lock);
    q->items = CreateSemaphore(NULL, 0, length, NULL);
    q->spaces = CreateSemaphore(NULL, length, length, NULL);
    return q;
}
int xQueueSend(QueueHandle_t handle, const void *data, unsigned timeout) {
    queue *q = handle;
    if (WaitForSingleObject(q->spaces, timeout) != WAIT_OBJECT_0)
        return 0;
    EnterCriticalSection(&q->lock);
    memcpy(q->data + q->tail * q->size, data, q->size);
    q->tail = (q->tail + 1) % q->length;
    LeaveCriticalSection(&q->lock);
    ReleaseSemaphore(q->items, 1, NULL);
    return 1;
}
int xQueueReceive(QueueHandle_t handle, void *data, unsigned timeout) {
    queue *q = handle;
    if (WaitForSingleObject(q->items, timeout) != WAIT_OBJECT_0)
        return 0;
    EnterCriticalSection(&q->lock);
    memcpy(data, q->data + q->head * q->size, q->size);
    q->head = (q->head + 1) % q->length;
    LeaveCriticalSection(&q->lock);
    ReleaseSemaphore(q->spaces, 1, NULL);
    return 1;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return CreateMutex(NULL, FALSE, NULL);
}
int xSemaphoreTake(SemaphoreHandle_t m, unsigned t) {
    return WaitForSingleObject(m, t) == WAIT_OBJECT_0;
}
int xSemaphoreGive(SemaphoreHandle_t m) {
    return ReleaseMutex(m);
}
typedef struct {
    void (*fn)(void *);
    void *arg;
} task_arg;
typedef struct {
    HANDLE signal;
    CRITICAL_SECTION lock;
    uint32_t value;
    bool pending;
} notification;
static DWORD notification_slot = TLS_OUT_OF_INDEXES;
static DWORD WINAPI task_entry(LPVOID value) {
    task_arg copy = *(task_arg *)value;
    free(value);
    copy.fn(copy.arg);
    return 0;
}
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                void *handle) {
    if (notification_slot == TLS_OUT_OF_INDEXES) notification_slot = TlsAlloc();
    (void)name;
    (void)stack;
    (void)priority;
    (void)handle;
    task_arg *a = malloc(sizeof(*a));
    if (!a)
        return 0;
    a->fn = fn;
    a->arg = arg;
    HANDLE thread = CreateThread(NULL, 0, task_entry, a, 0, NULL);
    if (!thread) {
        free(a);
        return 0;
    }
    CloseHandle(thread);
    return 1;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    if (notification_slot == TLS_OUT_OF_INDEXES) notification_slot = TlsAlloc();
    notification *n = TlsGetValue(notification_slot);
    if (!n) {
        n = calloc(1, sizeof(*n));
        n->signal = CreateEvent(NULL, TRUE, FALSE, NULL);
        InitializeCriticalSection(&n->lock);
        TlsSetValue(notification_slot, n);
    }
    return n;
}
TickType_t xTaskGetTickCount(void) {
    return (TickType_t)GetTickCount();
}
int xTaskNotify(TaskHandle_t task, uint32_t value, int action) {
    (void)action;
    notification *n = task;
    if (!n) return 0;
    EnterCriticalSection(&n->lock);
    n->value = value;
    n->pending = true;
    SetEvent(n->signal);
    LeaveCriticalSection(&n->lock);
    return 1;
}
int xTaskNotifyWait(uint32_t clear_on_entry, uint32_t clear_on_exit, uint32_t *value,
                    unsigned timeout) {
    notification *n = xTaskGetCurrentTaskHandle();
    DWORD began = GetTickCount();
    EnterCriticalSection(&n->lock);
    if (!n->pending) n->value &= ~clear_on_entry;
    LeaveCriticalSection(&n->lock);
    while (true) {
        EnterCriticalSection(&n->lock);
        if (n->pending) {
            if (value) *value = n->value;
            n->value &= ~clear_on_exit;
            n->pending = false;
            ResetEvent(n->signal);
            LeaveCriticalSection(&n->lock);
            return 1;
        }
        LeaveCriticalSection(&n->lock);
        DWORD elapsed = GetTickCount() - began;
        if (elapsed >= timeout || WaitForSingleObject(n->signal, timeout - elapsed) != WAIT_OBJECT_0)
            return 0;
    }
}
void vTaskDelay(unsigned ticks) {
    Sleep(ticks);
}
int64_t esp_timer_get_time(void) {
    return (int64_t)GetTickCount() * 1000;
}
static volatile LONG levels[40];
int gpio_config(const gpio_config_t *c) {
    for (int i = 0; i < 40; i++)
        if (c->pin_bit_mask & (1ULL << i))
            InterlockedExchange(&levels[i], c->pull_up_en ? 1 : 0);
    return ESP_OK;
}
int gpio_get_level(int pin) {
    return InterlockedCompareExchange(&levels[pin], 0, 0);
}
int gpio_set_level(int pin, int level) {
    InterlockedExchange(&levels[pin], level);
    return ESP_OK;
}
void stub_gpio_input(int pin, int value) {
    gpio_set_level(pin, value);
}
int uart_param_config(int port, const uart_config_t *c) {
    (void)port;
    (void)c;
    return 0;
}
int uart_set_pin(int p, int tx, int rx, int rts, int cts) {
    (void)p;
    (void)tx;
    (void)rx;
    (void)rts;
    (void)cts;
    return 0;
}
int uart_driver_install(int p, int rx, int tx, int q, void *h, int f) {
    (void)p;
    (void)rx;
    (void)tx;
    (void)q;
    (void)h;
    (void)f;
    return 0;
}
int uart_read_bytes(int p, void *b, unsigned s, unsigned t) {
    (void)p;
    (void)b;
    (void)s;
    (void)t;
    return 0;
}
int uart_write_bytes(int p, const void *b, size_t s) {
    (void)p;
    (void)b;
    return (int)s;
}
static uint32_t boot;
int nvs_flash_init(void) {
    return ESP_OK;
}
int nvs_open(const char *n, int m, nvs_handle_t *h) {
    (void)n;
    (void)m;
    *h = 1;
    return 0;
}
int nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *v) {
    (void)h;
    (void)k;
    *v = boot;
    return boot ? 0 : ESP_ERR_NVS_NOT_FOUND;
}
int nvs_set_u32(nvs_handle_t h, const char *k, uint32_t v) {
    (void)h;
    (void)k;
    boot = v;
    return 0;
}
int nvs_commit(nvs_handle_t h) {
    (void)h;
    return 0;
}
void nvs_close(nvs_handle_t h) {
    (void)h;
}
