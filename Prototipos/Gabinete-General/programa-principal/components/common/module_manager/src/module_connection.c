#include "module_connection.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "gpio_definition.h"
#include "driver/gptimer.h"

#define ESP_INTR_FLAG_DEFAULT 0
#define CONNECTION_WAIT_MS 1000 // Debounce time for connection stability

static const char* CONNECTION_TAG = "Module Connection";

static TaskHandle_t xTaskConnectionUpdate_handle = NULL;
static SemaphoreHandle_t xConnectionStateMutex = NULL;
static gptimer_handle_t debounce_timer = NULL;

static bool gpio_isr_service_installed = false;
static module_connection_state_t connection_state = CONNECTION_INIT;

// Callback function pointer for connection changes
static connection_callback_t connection_change_callback = NULL;

// Public function declarations (available to other modules)
void initialize_module_connection(void);
void register_connection_callback(connection_callback_t callback);
module_connection_state_t get_connection_state(void);

// Private function declarations
static void hp_isr_handler(void* arg);
static void setup_hotplug_gpios(void);
//static void delete_hotplug_gpios(void);
static void create_connection_tasks(void);
static void vTaskConnectionUpdate(void *pvParameters);
static void configure_debounce_timer(void);
static bool debounce_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
static void set_connection_state(module_connection_state_t new_state);

// Public functions
void initialize_module_connection(){
    create_connection_tasks();
    configure_debounce_timer();
}

void register_connection_callback(connection_callback_t callback) {
    connection_change_callback = callback;
}

module_connection_state_t get_connection_state() {
    module_connection_state_t state;
    if (xConnectionStateMutex != NULL && xSemaphoreTake(xConnectionStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = connection_state;
        xSemaphoreGive(xConnectionStateMutex);
    } else {
        state = CONNECTION_FAULT;
    }
    return state;
}

// Private functions
static void set_connection_state(module_connection_state_t new_state) {
    if (xConnectionStateMutex != NULL && xSemaphoreTake(xConnectionStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        connection_state = new_state;
        xSemaphoreGive(xConnectionStateMutex);
    }
}


static bool IRAM_ATTR debounce_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Notify connection task to check stable connection
    xTaskNotifyFromISR(xTaskConnectionUpdate_handle, CONNECTION_EVENT_STABLE, eSetBits, &xHigherPriorityTaskWoken);
    
    return (xHigherPriorityTaskWoken == pdTRUE);
}

static void configure_debounce_timer(){
    gptimer_config_t gptimer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000 * 1000, // 1 MHz resolution
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&gptimer_config, &debounce_timer)); 

    gptimer_alarm_config_t wait_alarm_config = {
        .alarm_count = CONNECTION_WAIT_MS * 1000, // 3 second debounce
        .flags.auto_reload_on_alarm = false,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(debounce_timer, &wait_alarm_config));

    gptimer_event_callbacks_t wait_callbacks = {
        .on_alarm = debounce_timer_cb, 
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(debounce_timer, &wait_callbacks, NULL));

    ESP_ERROR_CHECK(gptimer_enable(debounce_timer));
}

static void setup_hotplug_gpios(){
    gpio_config_t hp_pin_conf = {
        .pin_bit_mask = (1ULL << PIN_HP_POWER) | (1ULL << PIN_HP_SIGNAL), // Configure both HP pins
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&hp_pin_conf)); // Apply the configuration

    // Only install ISR service once
    if (!gpio_isr_service_installed) {
        ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM)); // Install ISR service
        gpio_isr_service_installed = true;
    }
    
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_HP_POWER, hp_isr_handler, NULL)); // Add ISR handler for HP Power pin
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_HP_SIGNAL, hp_isr_handler, NULL)); // Add ISR handler for HP Signal pin
}

/*
static void delete_hotplug_gpios(){
    // Only remove handlers if ISR service is installed
    if (gpio_isr_service_installed) {
        ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_HP_POWER)); // Remove the ISR handler for the HP Power pin
        ESP_ERROR_CHECK(gpio_isr_handler_remove(PIN_HP_SIGNAL)); // Remove the ISR handler for the HP Signal pin
        gpio_uninstall_isr_service(); // Uninstall the ISR service
        gpio_isr_service_installed = false; // Update the flag to reflect service is uninstalled
    }

    // Reset the GPIOs to their default state
    gpio_reset_pin(PIN_HP_POWER);
    gpio_reset_pin(PIN_HP_SIGNAL);
}*/

static void IRAM_ATTR hp_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    xTaskNotifyFromISR(xTaskConnectionUpdate_handle, CONNECTION_EVENT_CHANGE, eSetBits, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void create_connection_tasks() {
    xConnectionStateMutex = xSemaphoreCreateMutex();
    if (xConnectionStateMutex == NULL) {
        ESP_LOGE(CONNECTION_TAG, "Failed to create connection state mutex");
        return;
    }
    
    BaseType_t xReturned = xTaskCreate(vTaskConnectionUpdate,
                                       "Module Connection Task",
                                       4096,
                                       NULL,
                                       5,
                                       &xTaskConnectionUpdate_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(CONNECTION_TAG, "Failed to create Module Connection Task");
        set_connection_state(CONNECTION_FAULT);
    }
}

static void vTaskConnectionUpdate(void *pvParameters) {
    uint32_t notification_value;
    module_connection_state_t current_state;
    bool interrupts_disabled = false;
    
    while (true) {
        // Re-enable interrupts if they were disabled in previous iteration
        // Do this FIRST before any other operations
        if (interrupts_disabled) {
            gpio_intr_enable(PIN_HP_POWER);
            gpio_intr_enable(PIN_HP_SIGNAL);
            interrupts_disabled = false;
        }
        
        current_state = get_connection_state();
        
        if(current_state == CONNECTION_INIT) {
            ESP_LOGI(CONNECTION_TAG, "Module connection is initializing...");
            setup_hotplug_gpios();
            set_connection_state(CONNECTION_DISCONNECTED);
            // Give a small delay to let system stabilize after GPIO setup
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Wait for notification with timeout
        if (xTaskNotifyWait(0, UINT32_MAX, &notification_value, portMAX_DELAY) == pdTRUE) {
            
            // Clear any additional notifications that arrived while we were waking up
            // This ensures we process everything in one go without interruptions
            uint32_t extra_notifications = 0;
            while (xTaskNotifyWait(0, UINT32_MAX, &extra_notifications, 0) == pdTRUE) {
                notification_value |= extra_notifications;
            }
            
            if (notification_value & CONNECTION_EVENT_CHANGE) {
                // Temporarily disable interrupts to prevent rapid retriggering during processing
                gpio_intr_disable(PIN_HP_POWER);
                gpio_intr_disable(PIN_HP_SIGNAL);
                interrupts_disabled = true;
                
                // Small delay to ensure any pending ISR completes
                vTaskDelay(1);
                
                // GPIO state changed - check current state
                bool hp_power_level = gpio_get_level(PIN_HP_POWER);
                bool hp_signal_level = gpio_get_level(PIN_HP_SIGNAL);
                
                module_connection_state_t new_state;
                
                if (!hp_power_level && !hp_signal_level) {
                    new_state = CONNECTION_DETECTING;
                    gptimer_start(debounce_timer); // Start debounce timer
                } else {
                    new_state = CONNECTION_DISCONNECTED;
                    gptimer_stop(debounce_timer); // Stop debounce timer
                    gptimer_set_raw_count(debounce_timer, 0); // Reset timer count
                }
                
                current_state = get_connection_state();
                
                if (current_state != new_state) {
                    set_connection_state(new_state);
                    
                    switch (new_state) {
                        case CONNECTION_DISCONNECTED:
                            ESP_LOGI(CONNECTION_TAG, "Module disconnected");
                            if (connection_change_callback) {
                                connection_change_callback(false);
                            }
                            break;
                            
                        case CONNECTION_DETECTING:
                            ESP_LOGI(CONNECTION_TAG, "Module connection detected, waiting for stable connection...");
                            break;
                            
                        default:
                            break;
                    }
                }
                // Interrupts will be re-enabled at the top of the loop
            }
            
            if (notification_value & CONNECTION_EVENT_STABLE) {
                // Stop and reset timer first to prevent re-triggering
                gptimer_stop(debounce_timer);
                gptimer_set_raw_count(debounce_timer, 0);
                
                // Debounce timer expired - check if still connected
                bool hp_power_level = gpio_get_level(PIN_HP_POWER);
                bool hp_signal_level = gpio_get_level(PIN_HP_SIGNAL);
                
                if (!hp_power_level && !hp_signal_level) {
                    // Still connected after debounce period
                    set_connection_state(CONNECTION_CONNECTED);
                    ESP_LOGI(CONNECTION_TAG, "Module connection confirmed");
                    
                    if (connection_change_callback) {
                        connection_change_callback(true);
                    }
                } else {
                    // Connection lost during debounce
                    set_connection_state(CONNECTION_DISCONNECTED);
                    ESP_LOGI(CONNECTION_TAG, "Module connection lost during debounce");
                }
            }
        }
    }
}