/** file:       aclock.h
  * author:     <<AUTHOR>>
  */

#ifndef __CLOCK_H__
#define __CLOCK_H__

//___ I N C L U D E S ________________________________________________________

//___ M A C R O S ____________________________________________________________

//___ T Y P E D E F S ________________________________________________________

typedef struct aclock_state_t {
    /* similar to rtc_calendar_time */
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    bool     pm;
    /** Day value, where day 1 is the first day of the month. */
    uint8_t  day;
    /** Month value, where month 1 is January. */
    uint8_t  month;
    uint16_t year;
} aclock_state_t;

//___ V A R I A B L E S ______________________________________________________

aclock_state_t aclock_state;

//___ P R O T O T Y P E S ____________________________________________________

void aclock_enable ( void );
  /* @brief enable clock module (i.e. wake up from sleep)
   * @retrn None
   */

void aclock_disable ( void );
  /* @brief disable clock module (i.e. before sleeping)
   * @retrn None
   */

void aclock_set_time( uint8_t hour, uint8_t minute, uint8_t second);
  /* @brief set current time
   * @param user-provided ptrs to be filled
   * @retrn None
   */

int32_t aclock_get_timestamp ( void );
  /* @brief get unix time from RTC
   * @param None
   * @retrn unix time
   */

int32_t aclock_get_timestamp_relative( void );
  /* @brief Get the current timestamp as the number of seconds elapsed
   * since startdate (startdate is stored in flash) 
   * @return relative timestamp
   */
  
void aclock_get_time( uint8_t* hour_ptr, uint8_t* minute_ptr, uint8_t* second_ptr);
  /* @brief get current time
   * @param user-provided ptrs to be filled
   * @retrn None
   */

void aclock_init( void );
  /* @brief initalize clock module
   * @param callback to trigger on each clock tick (every second)
   * @retrn None
   */



#endif /* end of include guard: __CLOCK_H__ */

// vim:shiftwidth=2
