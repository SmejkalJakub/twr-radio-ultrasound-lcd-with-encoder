#include <application.h>

#define BATTERY_UPDATE_INTERVAL (15 * 60 * 1000)
#define ULTRASOUND_UPDATE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_INTERVAL (5 * 1000)

#define ULTRASOUND_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define ULTRASOUND_PUB_VALUE_CHANGE 20

#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.5f

#define MAX_DISTANCE 400

twr_led_t led;
twr_button_t button;
twr_tmp112_t tmp112;
twr_hc_sr04_t sr04;

bool sleep_active = false;

twr_button_t button_left;
twr_button_t button_right;

twr_tick_t distance_next_pub;
twr_tick_t temp_next_pub;


float distance = 0;
float last_published_distance = 0;
int alarm_distance = 50;
float temperature = 0;
float last_published_temp = 0;

void sleep()
{
    twr_module_lcd_clear();
    twr_module_lcd_update();
    sleep_active = true;
    twr_scheduler_plan_absolute(0, TWR_TICK_INFINITY);
}

void wakeup()
{
    sleep_active = false;
    twr_scheduler_plan_now(0);
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;

    if (sleep_active)
    {
        if (self == &button_left && event == TWR_BUTTON_EVENT_HOLD)
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
        if (self == &button_left && event == TWR_BUTTON_EVENT_HOLD)
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

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        float voltage = NAN;

        twr_module_battery_get_voltage(&voltage);
    }
}

void encoder_event_handler(twr_module_encoder_event_t event, void *event_param)
{
    if(event == TWR_MODULE_ENCODER_EVENT_ROTATION)
    {
        int increment = twr_module_encoder_get_increment();

        alarm_distance += increment;

        if(alarm_distance < 2)
        {
            alarm_distance = 2;
        }
        if(alarm_distance > MAX_DISTANCE)
        {
            alarm_distance = MAX_DISTANCE - 1;
        }

        twr_log_debug("alarm_distance %d", alarm_distance);
    }
}

void hc_sr04_event_handler(twr_hc_sr04_t *self, twr_hc_sr04_event_t event, void *event_param)
{
    float value;
    bc_log_info("Reading distance...");

    if (event != TWR_HC_SR04_EVENT_UPDATE)
    {
        twr_log_info("Problem with distance update");
        return;
    }


    if (twr_hc_sr04_get_distance_millimeter(&sr04, &value))
    {
        distance = value / 10;

        if(distance <= alarm_distance)
        {
            bool alarm = true;
            twr_radio_pub_float("distance/alarm/cm", &distance);
            twr_radio_pub_bool("alarm", &alarm);
        }

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
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    twr_hc_sr04_init(&sr04, TWR_GPIO_P9, TWR_GPIO_P8);
    twr_hc_sr04_set_event_handler(&sr04, hc_sr04_event_handler, NULL);
    twr_hc_sr04_set_update_interval(&sr04, ULTRASOUND_UPDATE_INTERVAL);

    // Init LCD
    twr_module_lcd_init();
    twr_module_lcd_set_font(&twr_font_ubuntu_13);
    twr_module_lcd_update();

    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_INTERVAL);

    const twr_button_driver_t* lcdButtonDriver =  twr_module_lcd_get_button_driver();
    twr_button_init_virtual(&button_left, 0, lcdButtonDriver, 0);
    twr_button_init_virtual(&button_right, 1, lcdButtonDriver, 0);

    twr_button_set_event_handler(&button_left, button_event_handler, (int*)0);
    twr_button_set_event_handler(&button_right, button_event_handler, (int*)1);

    twr_button_set_hold_time(&button_left, 300);
    twr_button_set_hold_time(&button_right, 300);

    twr_button_set_debounce_time(&button_left, 30);
    twr_button_set_debounce_time(&button_left, 30);

    twr_module_encoder_init();
    twr_module_encoder_set_event_handler(encoder_event_handler, NULL);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("ultrasound-distance", VERSION);
    
    twr_led_pulse(&led, 2000);
}

void application_task(void)
{
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

    snprintf(buffer, sizeof(buffer), "TEST");
    twr_module_lcd_set_font(&twr_font_ubuntu_15);
    twr_module_lcd_draw_string(8, 0, "DISTANCE METER", 1);
    twr_module_lcd_draw_string(0, 15, "--------------------------", 1);

    snprintf(buffer, sizeof(buffer), "Alarm: %d cm", alarm_distance);
    twr_module_lcd_draw_string(0, 25, buffer, 1);

    snprintf(buffer, sizeof(buffer), "Distance: %.1f cm", distance);
    twr_module_lcd_draw_string(0, 37, buffer, 1);

    snprintf(buffer, sizeof(buffer), "Temp: %.1f °C", temperature);
    twr_module_lcd_draw_string(0, 50, buffer, 1);

    twr_module_lcd_update();

    twr_system_pll_disable();

    twr_scheduler_plan_current_relative(200);
}
