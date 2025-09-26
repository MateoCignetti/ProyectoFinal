#include "computer_state.h"
#include "computer_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static computer_state_t computer_state = INIT; // Initial state of the computer

computer_state_t get_computer_state() {
    return computer_state; // Return the current state of the computer
}

void set_computer_state(computer_state_t new_state) {
    computer_state = new_state; // Set the new state of the computer
}

void vTaskUpdateComputerState(void *pvParameters) {
    // This task will periodically update the computer state
    TickType_t xLastWakeTime;
    while (1) {
        // Check the current state and perform actions accordingly
        switch (computer_state) {
            case INIT:
                // Perform initialization tasks
                //initialize_computer();
                break;
            case READY_FOR_MODULE:
                // Display a messsage on display. WIP.
                // Signal Module to start
                break;
            case MODULE_RUNNING:
                // Case when module signals it's running
                // When module signals it's disconnected, set state to READY_FOR_MODULE
                break;
            case FAULT:
                // Stop all operation and display an error message
                break;
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second before the next update
    }
}