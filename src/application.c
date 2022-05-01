#include <application.h>

// Time intervals for each sensor and module
#define BATTERY_UPDATE_INTERVAL (15 * 60 * 1000)
#define ULTRASOUND_UPDATE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_INTERVAL (5 * 1000)

#define ULTRASOUND_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define ULTRASOUND_PUB_VALUE_CHANGE 20

#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.5f

// Settings for the currently used ultrasound sensor
#define MIN_DISTANCE 2
#define MAX_DISTANCE 400

// Core Module LED instance
twr_led_t led;
twr_led_t lcdLedRed;

// Core Module temperature sensor instance
twr_tmp112_t tmp112;

// HC-SR04 ultrasound sensor instance
twr_hc_sr04_t sr04;

// LCD buttons instances
twr_button_t button_left;
twr_button_t button_right;

// Info about the next time the data should be published over the radio
twr_tick_t distance_next_pub;
twr_tick_t temp_next_pub;

// Some helper global variables
bool sleep_active = false;
bool alarm_armed = false;

float distance = 0;
float last_published_distance = 0;
int alarm_distance = 50;

float temperature = 0;
float last_published_temp = 0;

/**
 * @brief Switch the device to the sleep mode to preserve energy of batteries
 * 
 */
void sleep()
{
    twr_module_lcd_clear();
    twr_module_lcd_update();
    sleep_active = true;
    twr_scheduler_plan_absolute(0, TWR_TICK_INFINITY);
}

/**
 * @brief Wakeup the device from the sleep mode
 * 
 */
void wakeup()
{
    sleep_active = false;
    twr_scheduler_plan_now(0);
}

/**
 * @brief Callback that is some event on specific button.
 * 
 * @param self - instance of the button that was pressed/clicked/etc
 * @param event - specific event that occurred on the button. Possible values are TWR_BUTTON_EVENT_PRESS, TWR_BUTTON_EVENT_RELEASE, TWR_BUTTON_EVENT_CLICK or TWR_BUTTON_EVENT_HOLD
 * @param event_param - additional possible param that can be handled in this function on each event
 */
void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;

    if(self == &button_left && event == TWR_BUTTON_EVENT_HOLD)
    {
        alarm_armed = !alarm_armed;
        return;
    }

    if (sleep_active)
    {
        if (event == TWR_BUTTON_EVENT_HOLD)
        {
            wakeup();
        }
        else
        {
            return;
        }
    }
    else
    {
        if (self == &button_right && event == TWR_BUTTON_EVENT_HOLD)
        {
            sleep();
        }
        else
        {
            return;
        }
    }

    twr_scheduler_plan_now(0);
}

/**
 * @brief Callback that is called when there is update on the tmp112 sensor.
 * 
 * @param self - instance of tmp112 for that the event occurred
 * @param event - event that called this callback. It can be TWR_TMP112_EVENT_UPDATE or TWR_TMP112_EVENT_ERROR.
 * @param event_param - additional possible param that can be handled in this function on each event
 */
void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        float measured_temp;
        if (twr_tmp112_get_temperature_celsius(self, &measured_temp))
        {
            temperature = measured_temp;
            if ((fabs(measured_temp - last_published_temp) >= TEMPERATURE_PUB_VALUE_CHANGE) || (temp_next_pub < twr_scheduler_get_spin_tick()))
            {
                twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &measured_temp);
                last_published_temp = measured_temp;
                temp_next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}


/**
 * @brief Callback that is called when there is update on the Battery Module.
 * 
 * @param event - specific event that occurred on the Battery Module. Possible values are TWR_MODULE_BATTERY_EVENT_UPDATE, TWR_MODULE_BATTERY_EVENT_ERROR, TWR_MODULE_BATTERY_EVENT_LEVEL_LOW, TWR_MODULE_BATTERY_EVENT_LEVEL_CRITICAL
 * @param event_param - additional possible param that can be handled in this function on each event
 */
void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage = NAN;

        twr_module_battery_get_voltage(&voltage);
    }
}

/**
 * @brief Callback that is called when there is update on the Encoder knob on the Encoder Module.
 * 
 * @param event - specific event that occurred on the Encoder knob. Possible values are TWR_MODULE_ENCODER_EVENT_ROTATION, TWR_MODULE_ENCODER_EVENT_PRESS, TWR_MODULE_ENCODER_EVENT_RELEASE, TWR_MODULE_ENCODER_EVENT_CLICK, TWR_MODULE_ENCODER_EVENT_HOLD, TWR_MODULE_ENCODER_EVENT_ERROR
 * @param event_param - additional possible param that can be handled in this function on each event
 */
void encoder_event_handler(twr_module_encoder_event_t event, void *event_param)
{
    if(event == TWR_MODULE_ENCODER_EVENT_ROTATION)
    {
        int increment = twr_module_encoder_get_increment();

        alarm_distance += increment;

        if(alarm_distance < MIN_DISTANCE)
        {
            alarm_distance = MIN_DISTANCE;
        }
        if(alarm_distance > MAX_DISTANCE)
        {
            alarm_distance = MAX_DISTANCE;
        }
    }
}

/**
 * @brief 
 * 
 * @param self - instance of the ultrasound sensor
 * @param event - specific event that occurred on the ultrasound sensor. Possible values are TWR_HC_SR04_EVENT_UPDATE, TWR_HC_SR04_EVENT_ERROR.
 * @param event_param - additional possible param that can be handled in this function on each event
 */
void hc_sr04_event_handler(twr_hc_sr04_t *self, twr_hc_sr04_event_t event, void *event_param)
{
    float value;

    if (event != TWR_HC_SR04_EVENT_UPDATE)
    {
        return;
    }

    if (twr_hc_sr04_get_distance_millimeter(&sr04, &value))
    {
        distance = value / 10;

        // Publish data about the alarm if the object is closer than the set alarm distance
        if((distance <= alarm_distance) && alarm_armed)
        {
            bool alarm = true;

            twr_led_blink(&lcdLedRed, 3);
            twr_radio_pub_float("distance/alarm/cm", &distance);
            twr_radio_pub_bool("alarm", &alarm);
        }

        // Publish the distance over the radio only every few minutes or if there is significant change
        if ((fabs((value / 10) - last_published_distance) >= ULTRASOUND_PUB_VALUE_CHANGE) || (distance_next_pub < twr_scheduler_get_spin_tick()))
        {
            last_published_distance = value / 10;
            distance_next_pub = twr_scheduler_get_spin_tick() + ULTRASOUND_PUB_NO_CHANGE_INTEVAL;
            twr_log_info("Distance send %f cm", last_published_distance);

            twr_radio_pub_float("distance/-/cm", &last_published_distance);
        }
        twr_log_info("Distance measured %f cm", distance);

    }
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

}

/**
 * @brief This function is called once at the power up of the device. It takes care of all the initializations and module preparations.
 * Equivalent to the arduino setup function.
 */
void application_init(void)
{
    // Initialize Logging
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize Battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize HC-SR04 ultrasound sensor
    twr_hc_sr04_init(&sr04, TWR_GPIO_P9, TWR_GPIO_P8);
    twr_hc_sr04_set_event_handler(&sr04, hc_sr04_event_handler, NULL);
    twr_hc_sr04_set_update_interval(&sr04, ULTRASOUND_UPDATE_INTERVAL);

    // Initialize LCD
    twr_module_lcd_init();
    twr_module_lcd_set_font(&twr_font_ubuntu_13);
    twr_module_lcd_update();

    const twr_led_driver_t* driver = twr_module_lcd_get_led_driver();
    twr_led_init_virtual(&lcdLedRed, TWR_MODULE_LCD_LED_RED, driver, 1);

    // Initialize TMP-112 temperature sensor on Core Module
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_INTERVAL);

    // Initialize LCD Module buttons
    const twr_button_driver_t* lcdButtonDriver =  twr_module_lcd_get_button_driver();
    twr_button_init_virtual(&button_left, 0, lcdButtonDriver, 0);
    twr_button_init_virtual(&button_right, 1, lcdButtonDriver, 0);
    twr_button_set_event_handler(&button_left, button_event_handler, (int*)0);
    twr_button_set_event_handler(&button_right, button_event_handler, (int*)1);

    twr_button_set_hold_time(&button_left, 300);
    twr_button_set_hold_time(&button_right, 300);

    twr_button_set_debounce_time(&button_left, 30);
    twr_button_set_debounce_time(&button_right, 30);

    // Initialize Encoder
    twr_module_encoder_init();
    twr_module_encoder_set_event_handler(encoder_event_handler, NULL);

    // Initialize radio communication 
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("ultrasound-distance", VERSION);
    
    twr_led_pulse(&led, 2000);
}

/**
 * @brief This function is called repeatedly every 200ms or every 20ms if the LCD module is not ready to be updated
 * It takes care for all LCD writing. If the device is in the sleep mode to preserve power the function will do nothing.
 * Equivalent to the arduino loop function.
 */
void application_task(void)
{
    // Replan the update if the LCD Module is not ready
    if (!twr_module_lcd_is_ready())
    {
        twr_scheduler_plan_current_relative(20);
        return;
    }

    if(sleep_active)
    {
        return;
    }

    twr_system_pll_enable();

    char buffer[30];
    twr_module_lcd_clear();

    // All the strings that are printed out onto the LCD
    twr_module_lcd_set_font(&twr_font_ubuntu_15);
    twr_module_lcd_draw_string(8, 0, "DISTANCE METER", 1);
    twr_module_lcd_draw_string(0, 15, "--------------------------", 1);

    snprintf(buffer, sizeof(buffer), "Distance: %.1f cm", distance);
    twr_module_lcd_draw_string(0, 25, buffer, 1);

    snprintf(buffer, sizeof(buffer), "Alarm: %d cm", alarm_distance);
    twr_module_lcd_draw_string(0, 37, buffer, 1);

    snprintf(buffer, sizeof(buffer), "Temp: %.1f °C", temperature);
    twr_module_lcd_draw_string(0, 50, buffer, 1);

    if(alarm_armed)
    {
        twr_module_lcd_draw_string(38, 70, "ARMED", 1);

        twr_module_lcd_set_font(&twr_font_ubuntu_11);
        twr_module_lcd_draw_string(3, 98, "HOLD", 1);
        twr_module_lcd_draw_string(7, 108, "FOR", 1);
        twr_module_lcd_draw_string(2, 118, "DISARM", 1);
    }
    else
    {
        twr_module_lcd_draw_string(30, 70, "DISARMED", 1);

        twr_module_lcd_set_font(&twr_font_ubuntu_11);
        twr_module_lcd_draw_string(3, 98, "HOLD", 1);
        twr_module_lcd_draw_string(7, 108, "FOR", 1);
        twr_module_lcd_draw_string(6, 118, "ARM", 1);
    }

    twr_module_lcd_draw_string(100, 98, "HOLD", 1);
    twr_module_lcd_draw_string(104, 108, "FOR", 1);
    twr_module_lcd_draw_string(100, 118, "SLEEP", 1);

    // Whole LCD update
    twr_module_lcd_update();

    twr_system_pll_disable();

    twr_scheduler_plan_current_relative(200);
}
