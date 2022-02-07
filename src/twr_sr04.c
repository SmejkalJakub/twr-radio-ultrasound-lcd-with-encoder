#include <twr_sr04.h>

#define _TWR_SR04_DELAY_RUN 3000


static void _twr_sr04_task_interval(void *param);
static void _twr_sr04_task_measure(void *param);

void twr_sr04_init(twr_sr04_t *self, twr_uart_channel_t uart_channel)
{
    memset(self, 0, sizeof(*self));

    self->_uart_channel = uart_channel;

    self->_task_id_interval = twr_scheduler_register(_twr_sr04_task_interval, self, TWR_TICK_INFINITY);
    self->_task_id_measure = twr_scheduler_register(_twr_sr04_task_measure, self, _TWR_SR04_DELAY_RUN);
}

void twr_sr04_set_event_handler(twr_sr04_t *self, void (*event_handler)(twr_sr04_t *, twr_sr04_event_t, void *), void *event_param)
{
    self->_event_handler = event_handler;
    self->_event_param = event_param;
}

void twr_sr04_set_update_interval(twr_sr04_t *self, twr_tick_t interval)
{
    self->_update_interval = interval;

    if (self->_update_interval == TWR_TICK_INFINITY)
    {
        twr_scheduler_plan_absolute(self->_task_id_interval, TWR_TICK_INFINITY);
    }
    else
    {
        twr_scheduler_plan_relative(self->_task_id_interval, self->_update_interval);

        twr_sr04_measure(self);
    }
}

bool twr_sr04_measure(twr_sr04_t *self)
{   
    if (self->_measurement_active)
    {
        return false;
    }

    self->_measurement_active = true;

    twr_scheduler_plan_absolute(self->_task_id_measure, self->_tick_ready);

    return true;
}

static void _twr_sr04_task_interval(void *param)
{
    twr_sr04_t *self = param;

    twr_sr04_measure(self);

    twr_scheduler_plan_current_relative(self->_update_interval);
}

static void _twr_sr04_task_measure(void *param)
{
    twr_sr04_t *self = param;

start:

    switch (self->_state)
    {
        case TWR_SR04_STATE_ERROR:
        {
            self->_measurement_active = false;

            if (self->_event_handler != NULL)
            {
                self->_event_handler(self, TWR_SR04_EVENT_ERROR, self->_event_param);
            }

            self->_state = TWR_SR04_STATE_INITIALIZE;

            return;
        }
        default:
        {
            self->_state = TWR_SR04_STATE_ERROR;

            goto start;
        }
    }
}