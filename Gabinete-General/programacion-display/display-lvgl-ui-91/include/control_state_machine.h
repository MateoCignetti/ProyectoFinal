#ifndef CONTROL_STATE_MACHINE_H
#define CONTROL_STATE_MACHINE_H

// 1. DEFINE EL TIPO ENUM AQUÍ.
// Así, cualquier archivo que incluya "control.h" sabrá qué es "control_mode_t".
typedef enum {
    CONTROL_MODE_IDLE,
    CONTROL_MODE_FIXED_PWM,
    CONTROL_MODE_PID
} control_mode_t;

// 2. DECLARA LA VARIABLE CON 'extern' AQUÍ.
// Esto anuncia que una variable llamada 'g_control_mode' existe en alguna parte.
extern volatile control_mode_t control_mode;

#endif // CONTROL_H