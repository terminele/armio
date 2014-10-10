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

aclock_tick_callback_t user_tick_cb = NULL;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

void rtc_alarm_s_callback( void ) {

    /* trigger user tick callback */
    if (user_tick_cb) user_tick_cb();

    /* Set next alarm for a second later */
    alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;
    alarm.time.second += 1;
    alarm.time.second = alarm.time.second % 60;

    rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);

}


//___ F U N C T I O N S ______________________________________________________


void aclock_get_state( aclock_state_t *clock_state ) {


}

void aclock_init( aclock_tick_callback_t tick_cb) {
    struct rtc_calendar_time time;

    user_tick_cb = tick_cb;

    /* Initialize RTC in calendar mode */
    struct rtc_calendar_config config_rtc_calendar;

    /* Base time defaults to 1/1/2000 @ 0:00 (midnight) */
    rtc_calendar_get_config_defaults(&config_rtc_calendar);

    /* Configure alarm to trigger at 1-second    */
    /* we only care about second since other     */
    /* fields are masked so can be garbage       */
    alarm.time.second = 1;
    alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;

    config_rtc_calendar.clock_24h = true;
    config_rtc_calendar.alarm[0].time = alarm.time;
    config_rtc_calendar.alarm[0].mask = RTC_CALENDAR_ALARM_MASK_YEAR;

    rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);
    rtc_calendar_enable(&rtc_instance);

    /* Register alarm callbacks */
    rtc_calendar_register_callback( &rtc_instance,
        rtc_alarm_s_callback, RTC_CALENDAR_CALLBACK_ALARM_0);

    rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_ALARM_0);


    /* Set current time defaults */
    rtc_calendar_get_time_defaults(&time);
    time.year   = 2014;
    time.month  = 10;
    time.day    = 10;
    time.hour   = 10;
    time.minute = 0;
    time.second = 0;

    rtc_calendar_set_time(&rtc_instance, &time);

    while(1) {};
}


// vim: shiftwidth=4
