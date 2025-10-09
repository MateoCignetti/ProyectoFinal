#ifndef DIMMER_INTERFACE_H
#define DIMMER_INTERFACE_H

/**
 * @brief Start the dimmer interface and UI
 *
 * Initializes LVGL UI, creates groups for screen navigation,
 * and starts the group update task.
 */
void start_dimmer_interface(void);

/**
 * @brief Stop the dimmer interface and clean up resources
 *
 * Deletes the update task, destroys LVGL groups and UI objects.
 */
void stop_dimmer_interface(void);

#endif // DIMMER_INTERFACE_H
