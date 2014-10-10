/** file:       clock.c
  * author:     Richard Bryan
  *
  * Derived from the atmel asf example qs_rtc_calendar_callback.c
  *
  */

//___ I N C L U D E S ________________________________________________________
#include <asf.h>
#include "aclock.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

void rtc_alarm_s_callback( void );
  /* @brief called on 1-second alarm trigger interrupt
   * then schedules next alarm
   * @param None
   * @retrn None
   */


//___ V A R I A B L E S ______________________________________________________
struct rtc_module rtc_instance;

struct rtc_calendar_alarm_time alarm;

aclock_state_t aclock_global_state;
aclock_tick_callback_t user_tick_cb = NULL;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

void rtc_alarm_s_callback( void ) {
    struct rtc_calendar_time curr_time;

    /* Set next alarm for a second later */
    alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;
    alarm.time.second += 1;
    alarm.time.second = alarm.time.second % 60;

    rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);

    /* Update our time state */
    rtc_calendar_get_time(&rtc_instance, &curr_time);
    aclock_global_state.second = curr_time.second;
    aclock_global_state.minute = curr_time.minute;
    aclock_global_state.hour = curr_time.hour;
    aclock_global_state.day = curr_time.day;
    aclock_global_state.month = curr_time.month;
    aclock_global_state.year = curr_time.year;

}


//___ F U N C T I O N S ______________________________________________________


void aclock_init( aclock_tick_callback_t tick_cb) {
    struct rtc_calendar_time initial_time;

    user_tick_cb = tick_cb;

    /* Initialize RTC in calendar mode */
    struct rtc_calendar_config config_rtc_calendar;

    /* Base time defaults to 1/1/2000 @ 0:00 (midnight) */
    rtc_calendar_get_config_defaults(&config_rtc_calendar);

    /* Set current time */
    rtc_calendar_get_time_defaults(&initial_time);
    initial_time.year   = aclock_global_state.year = 2014;
    initial_time.month  = aclock_global_state.month = 10;
    initial_time.day    = aclock_global_state.day = 10;

    /* Use compile time for initial time */
    initial_time.hour   = aclock_global_state.hour = 10*(__TIME__[0] - '0') +  (__TIME__[1] - '0');
    initial_time.minute = aclock_global_state.minute = 10*(__TIME__[3] - '0') +  (__TIME__[4] - '0');
    initial_time.second = aclock_global_state.second = (10*(__TIME__[6] - '0') +  (__TIME__[7] - '0') + \
                          10) % 60; //make it a little fast to account for time between compiling and flashing

    /* Configure alarm to trigger at 1-second    */
    /* we only care about second since other     */
    /* fields are masked so can be garbage       */
    alarm.time.second = initial_time.second;
    alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;

    config_rtc_calendar.clock_24h = true;
    config_rtc_calendar.alarm[0].time = alarm.time;
    config_rtc_calendar.alarm[0].mask = alarm.mask;

    rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);
    rtc_calendar_enable(&rtc_instance);

    /* Register alarm callbacks */
    rtc_calendar_register_callback( &rtc_instance,
        rtc_alarm_s_callback, RTC_CALENDAR_CALLBACK_ALARM_0);

    rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_ALARM_0);

    rtc_calendar_set_time(&rtc_instance, &initial_time);

}


// vim: shiftwidth=2
