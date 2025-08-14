#ifndef COMPUTER_STATE_H
#define COMPUTER_STATE_H

typedef enum {
    INIT,
    READY_FOR_MODULE,
    MODULE_RUNNING,
    FAULT
} computer_state_t;

computer_state_t get_computer_state(void);
void set_computer_state(computer_state_t new_state);

#endif