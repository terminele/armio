/** file:       clock.c
  * author:     Richard Bryan
  *
  * Derived from the atmel asf example qs_rtc_calendar_callback.c
  *
  */

//___ I N C L U D E S ________________________________________________________
#include <asf.h>
#include "aclock.h"
#include "main.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#ifndef ALARM_INTERVAL_MIN
#define ALARM_INTERVAL_MIN 1 
#endif
//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

void rtc_alarm_short_callback( void );
  /* @brief alarm callback
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
static struct rtc_module rtc_instance;

static struct rtc_calendar_alarm_time alarm;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

#ifdef USE_WAKEUP_ALARM

static void rtc_alarm_short_callback( void ) {

    aclock_enable();

    /* Set next alarm */
    alarm.time.minute += ALARM_INTERVAL_MIN;
    alarm.time.minute %= 60;

    rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);

}

#endif

static int32_t calc_timestamp(uint16_t year, uint8_t month, uint8_t day, 
    uint8_t hour, uint8_t minute, uint8_t second, bool pm) {

#define SECONDS_PER_DAY 86400
#define SECONDS_PER_YEAR SECONDS_PER_DAY*365
  int32_t value = ((uint32_t)year - 1970)*SECONDS_PER_YEAR;

  //account for extra day in leap years
  value += (((uint32_t)(year - 1970)/4)*SECONDS_PER_DAY);

  switch (month) {
    /* Each case accounts for the days of
     * the previous month */
    case 12:
      value+=SECONDS_PER_DAY*30;
    case 11://30
      value+=SECONDS_PER_DAY*31;
    case 10://31
      value+=SECONDS_PER_DAY*30;
    case 9://30
      value+=SECONDS_PER_DAY*31;
    case 8://31
      value+=SECONDS_PER_DAY*31;
    case 7://31
      value+=SECONDS_PER_DAY*30;
    case 6://30
      value+=SECONDS_PER_DAY*31;
    case 5://31
      value+=SECONDS_PER_DAY*30;
    case 4://30
      value+=SECONDS_PER_DAY*31;
    case 3://31
      //NOTE: leap year day accounted above for this year too
      value+=SECONDS_PER_DAY*28;
    case 2:
      value+=SECONDS_PER_DAY*31;
    case 1://31
      break;
    default:
      break;
  }

  value+=((uint32_t)day*SECONDS_PER_DAY);
  hour = hour % 12; //12am/pm should be 0 for below calculation
  value+=((uint32_t)hour + (pm ? 12 : 0))*3600;
  value+=((uint32_t)minute*60);
  value+=second;

  return value;
}

//___ F U N C T I O N S ______________________________________________________

void aclock_sync_ready_cb ( void ) {
    struct rtc_calendar_time curr_time;
    rtc_calendar_get_time(&rtc_instance, &curr_time);
    aclock_state.year = curr_time.year;
    aclock_state.month = curr_time.month;
    aclock_state.day = curr_time.day;

    aclock_state.hour = curr_time.hour;
    aclock_state.minute = curr_time.minute;
    aclock_state.second = curr_time.second;


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

    *hour_ptr = aclock_state.hour;
    *minute_ptr = aclock_state.minute;
    *second_ptr = aclock_state.second;


}

void aclock_enable ( void ) {

  rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_SYNCRDY);

  /* ###continuous update doesn't seeem to be working so... */
  /* Make another calendar read request */
  RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
  
  while (rtc_calendar_is_syncing(&rtc_instance)) {
          /* Wait for synchronization */
  }

}

int32_t aclock_get_timestamp ( void ) {
  /* haphazard caculation of unix timestamp from
    * RTC datatime.  May be wrong, but hey
    * this isnt a critical application */

  RTC->MODE2.READREQ.reg = RTC_READREQ_RREQ;
  while (rtc_calendar_is_syncing(&rtc_instance));
  struct rtc_calendar_time curr_time;
  rtc_calendar_get_time(&rtc_instance, &curr_time);
  aclock_state.year = curr_time.year;
  aclock_state.month = curr_time.month;
  aclock_state.day = curr_time.day;
  aclock_state.hour = curr_time.hour;
  aclock_state.minute = curr_time.minute;
  aclock_state.second = curr_time.second;
  aclock_state.pm = curr_time.pm;
  
  return calc_timestamp(aclock_state.year, aclock_state.month, aclock_state.day,
      aclock_state.hour, aclock_state.minute, aclock_state.second, aclock_state.pm);

}

int32_t aclock_get_timestamp_relative( void ) {
  /* Get the current timestamp as the number of seconds elapsed
   * since startdate (startdate is stored in flash) */
  
  int32_t startdate_ts = calc_timestamp(main_nvm_data.year, main_nvm_data.month,
      main_nvm_data.day, main_nvm_data.hour, main_nvm_data.minute, main_nvm_data.second, 
      main_nvm_data.pm);

  int32_t curr_ts = aclock_get_timestamp();
  
  return curr_ts - startdate_ts;
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
    
    if (main_nvm_data.second >= 0 && main_nvm_data.second < 60 &&
        main_nvm_data.minute >= 0 && main_nvm_data.minute < 60 &&
        main_nvm_data.hour > 0 && main_nvm_data.hour <= 12) {
      /* Datetime has been saved in nvm */
      initial_time.year   = aclock_state.year   = main_nvm_data.year;
      initial_time.month  = aclock_state.month  = main_nvm_data.month; 
      initial_time.day    = aclock_state.day    = main_nvm_data.day; 
                                                 
      initial_time.hour   = aclock_state.hour   = main_nvm_data.hour;
      initial_time.minute = aclock_state.minute = main_nvm_data.minute;
      initial_time.second = aclock_state.second = main_nvm_data.second;
      initial_time.pm     = aclock_state.pm     = main_nvm_data.pm;
    } else {
      /* Use compile time flags for initial time if not configured in flash */
      main_nvm_data.year   =  initial_time.year   = aclock_state.year   = __YEAR__;
      main_nvm_data.month  =  initial_time.month  = aclock_state.month  = __MONTH__;
      main_nvm_data.day    =  initial_time.day    = aclock_state.day    = __DAY__;
                           
      main_nvm_data.hour   =  initial_time.hour   = aclock_state.hour   =  __HOUR__;
      main_nvm_data.minute =  initial_time.minute = aclock_state.minute = __MIN__;
      main_nvm_data.second =  initial_time.second = aclock_state.second = __SEC__;
      main_nvm_data.pm     =  initial_time.pm     = aclock_state.pm     = __PM__;
    }

    config_rtc_calendar.clock_24h = false;
    config_rtc_calendar.prescaler  = RTC_CALENDAR_PRESCALER_DIV_1024;
    config_rtc_calendar.continuously_update = true;
    config_rtc_calendar.alarm[0] = alarm;

    rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);


    if ((uint8_t)main_nvm_data.rtc_freq_corr == 0xff) {
      main_nvm_data.rtc_freq_corr = 0;
    }
    if (STATUS_OK != rtc_calendar_frequency_correction(&rtc_instance, main_nvm_data.rtc_freq_corr)) {
      main_terminate_in_error(error_group_assertion, 0); 
    }
    
    rtc_calendar_enable(&rtc_instance);
    
    rtc_calendar_set_time(&rtc_instance, &initial_time);

    /* Register sync ready callback */
    rtc_calendar_register_callback( &rtc_instance,
        aclock_sync_ready_cb, RTC_CALENDAR_CALLBACK_SYNCRDY);

    rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_CALLBACK_SYNCRDY);
    

#ifdef USE_WAKEUP_ALARM
    /* Configure alarm  */
    alarm.time.second = initial_time.second; 
    alarm.time.minute = initial_time.minute + ALARM_INTERVAL_MIN;

    alarm.time.minute %= 60;


    alarm.mask = RTC_CALENDAR_ALARM_MASK_MIN;
    rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);

    /* Register ready callback */
    rtc_calendar_register_callback( &rtc_instance,
        rtc_alarm_short_callback, RTC_CALENDAR_ALARM_0);

    rtc_calendar_enable_callback(&rtc_instance, RTC_CALENDAR_ALARM_0);
#endif
}

// vim:shiftwidth=2
