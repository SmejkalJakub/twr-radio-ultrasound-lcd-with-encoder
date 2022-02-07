#include <application.h>

#define APPLICATION_TASK_INTERVAL (1 * 60 * 1000)

twr_led_t led;
twr_button_t button;

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_CLICK)
    {

    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {

    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage = NAN;

        twr_module_battery_get_voltage(&voltage);
    }
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    
    twr_led_pulse(&led, 2000);
}


void application_task(void)
{
    twr_scheduler_plan_current_relative(APPLICATION_TASK_INTERVAL);
}
