#ifndef BUCK_INTERFACE_H
#define BUCK_INTERFACE_H

/**
 * @brief Start the buck converter interface and UI
 * 
 * Initializes LVGL UI, creates groups for screen navigation,
 * and starts the group update task.
 */
void start_buck_interface(void);

/**
 * @brief Stop the buck converter interface and clean up resources
 * 
 * Deletes the update task, destroys LVGL groups and UI objects.
 */
void stop_buck_interface(void);

#endif // BUCK_INTERFACE_H