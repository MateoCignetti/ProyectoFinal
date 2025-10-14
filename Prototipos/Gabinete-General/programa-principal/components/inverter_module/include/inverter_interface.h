#ifndef INVERTER_INTERFACE_H
#define INVERTER_INTERFACE_H

/**
 * @brief Start the inverter interface and UI
 *
 * Initializes LVGL UI, creates groups for screen navigation,
 * and starts the group update task.
 */
void start_inverter_interface(void);

/**
 * @brief Stop the inverter interface and clean up resources
 *
 * Deletes the update task, destroys LVGL groups and UI objects.
 */
void stop_inverter_interface(void);

#endif // INVERTER_INTERFACE_H
