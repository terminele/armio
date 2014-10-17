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
void aclock_sync_ready_cb ( void );
  /* @brief callback after an RTC read sync
   * is finished and we can read an updated
   * clock value
   * @param None
   * @retrn None
   */


//___ V A R I A B L E S ______________________________________________________
struct rtc_module rtc_instance;

struct rtc_calendar_alarm_time alarm;
aclock_state_t global_state;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

#ifdef NOT_NOW
void rtc_alarm_s_callback( void ) {
    struct rtc_calendar_time curr_time;
    //user_tick_cb();
    /* Set next alarm for a second later */
    alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;
    alarm.time.second += 1;
    alarm.time.second = alarm.time.second % 60;

    rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);

    /* Update our time state */



}

#endif


//___ F U N C T I O N S ______________________________________________________

void aclock_sync_ready_cb ( void ) {
    struct rtc_calendar_time curr_time;
    rtc_calendar_get_time(&rtc_instance, &curr_time);
    global_state.year = curr_time.year;
    global_state.month = curr_time.month;
    global_state.day = curr_time.day;

    global_state.hour = curr_time.hour;
    global_state.minute = curr_time.minute;
    global_state.second = curr_time.second;


    /* ###continuous update doesn't seeem to be working so... */
    /* Make another read request */
    RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
}

void aclock_set_time( uint8_t hour, uint8_t minute, uint8_t second) {
    struct rtc_calendar_time curr_time;
    rtc_calendar_get_time(&rtc_instance, &curr_time);
    curr_time.hour = hour;
    curr_time.minute = minute;
    curr_time.second = second;
    rtc_calendar_set_time(&rtc_instance, &curr_time);
}


void aclock_get_time( uint8_t* hour_ptr, uint8_t* minute_ptr, uint8_t* second_ptr) {

#ifdef NOT_NOW
    struct rtc_calendar_time curr_time;
    if (!rtc_calendar_is_syncing(&rtc_instance)) {
      rtc_calendar_get_time(&rtc_instance, &curr_time);
      global_state.year = curr_time.year;
      global_state.month = curr_time.month;
      global_state.day = curr_time.day;

      global_state.hour = curr_time.hour;
      global_state.minute = curr_time.minute;
      global_state.second = curr_time.second;
    }
#endif

    *hour_ptr = global_state.hour;
    *minute_ptr = global_state.minute;
    *second_ptr = global_state.second;


}

void aclock_enable ( void ) {

  rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_SYNCRDY);
  rtc_calendar_enable(&rtc_instance);

  /* ###continuous update doesn't seeem to be working so... */
  /* Make another calendar read request */
  RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;

}

void aclock_disable ( void ) {

  rtc_calendar_disable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_SYNCRDY);
}

void aclock_init( void ) {
    struct rtc_calendar_time initial_time;


    /* Initialize RTC in calendar mode */
    struct rtc_calendar_config config_rtc_calendar;

    /* Base time defaults to 1/1/2000 @ 0:00 (midnight) */
    rtc_calendar_get_config_defaults(&config_rtc_calendar);

    /* Set current time */
    rtc_calendar_get_time_defaults(&initial_time);
    initial_time.year   = global_state.year = 2014;
    initial_time.month  = global_state.month = 10;
    initial_time.day    =  global_state.day = 10;

    /* Use compile time for initial time */
    initial_time.hour   = global_state.hour =  10*(__TIME__[0] - '0') +  (__TIME__[1] - '0');
    initial_time.minute = global_state.minute = 5 + 10*(__TIME__[3] - '0') +  (__TIME__[4] - '0');
    initial_time.second = global_state.second = 10*(__TIME__[6] - '0') +  (__TIME__[7] - '0') % 60;

#pragma message "Setting intial time to " __TIME__ " + 5 minutes"

    /* Configure alarm to trigger at 1-second    */
    /* we only care about second since other     */
    /* fields are masked so can be garbage       */
    alarm.time.second = initial_time.second;
    alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;

    config_rtc_calendar.clock_24h = true;
    config_rtc_calendar.prescaler  = RTC_CALENDAR_PRESCALER_DIV_1024;
    //config_rtc_calendar.alarm[0].time = alarm.time;
    //config_rtc_calendar.alarm[0].mask = alarm.mask;
    config_rtc_calendar.continuously_update = true;

    rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);

    rtc_calendar_enable(&rtc_instance);

    rtc_calendar_set_time(&rtc_instance, &initial_time);

    /* Register sync ready callback */
    rtc_calendar_register_callback( &rtc_instance,
        aclock_sync_ready_cb, RTC_CALENDAR_CALLBACK_SYNCRDY);

    rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_SYNCRDY);

}


// vim: shiftwidth=2
