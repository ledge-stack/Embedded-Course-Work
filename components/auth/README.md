# Shared authentication support

The PIN verifier supports Leon's keypad/dashboard and Edward's serial
administrator session. It uses a PBKDF2 verifier and a temporary lockout after
repeated failures. The fixed PIN is for this demonstration only. It is not
production-grade user authentication.
