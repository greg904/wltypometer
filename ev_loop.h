#ifndef WLTYPOMETER_EV_LOOP_H
#define WLTYPOMETER_EV_LOOP_H

/**
 * See ev_loop_timer.
 */
extern void (*ev_loop_timer_cb)();

/**
 * Time remaining in milliseconds before ev_loop_timer_cb is called. -1 means
 * the timer is disabled.
 */
extern int ev_loop_timer;

extern int ev_loop_error;

int ev_loop_run();

#endif
