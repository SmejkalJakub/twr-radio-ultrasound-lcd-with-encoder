#ifndef _SR04_H
#define _SR04_H

#include <twr_uart.h>
#include <twr_scheduler.h>

typedef struct twr_sr04_t twr_sr04_t;

typedef enum
{
    //! @brief Error event
    TWR_SR04_EVENT_ERROR = 0,

    //! @brief Update event
    TWR_SR04_EVENT_UPDATE = 1

} twr_sr04_event_t;

typedef enum
{
    TWR_SR04_STATE_ERROR = -1,
    TWR_SR04_STATE_INITIALIZE = 0,
    TWR_SR04_STATE_MEASURE = 1,
    TWR_SR04_STATE_READ = 2,
    TWR_SR04_STATE_UPDATE = 3

} twr_sr04_state_t;

struct twr_sr04_t
{
    twr_uart_channel_t _uart_channel;
    twr_scheduler_task_id_t _task_id_interval;
    twr_scheduler_task_id_t _task_id_measure;
    void (*_event_handler)(twr_sr04_t *, twr_sr04_event_t, void *);
    void *_event_param;
    bool _measurement_active;
    twr_tick_t _update_interval;
    twr_sr04_state_t _state;
    twr_tick_t _tick_ready;
};


void twr_sr04_init(twr_sr04_t *self, twr_uart_channel_t uart_channel);
void twr_sr04_set_event_handler(twr_sr04_t *self, void (*event_handler)(twr_sr04_t *, twr_sr04_event_t, void *), void *event_param);
void twr_sr04_set_update_interval(twr_sr04_t *self, twr_tick_t interval);
bool twr_sr04_measure(twr_sr04_t *self);

#endif // _SR04_H