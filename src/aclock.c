/** file:       clock.c
  * author:     <<AUTHOR>>
  */

//___ I N C L U D E S ________________________________________________________
#include <asf.h>
#include "aclock.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
void rtc_match_callback(void);
void configure_rtc_callbacks(void);
void configure_rtc_calendar(void);


//___ V A R I A B L E S ______________________________________________________
struct rtc_module rtc_instance;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________
void configure_rtc_calendar(void)
{
	/* Initialize RTC in calendar mode. */
	struct rtc_calendar_config config_rtc_calendar;
	rtc_calendar_get_config_defaults(&config_rtc_calendar);

	config_rtc_calendar.clock_24h = true;
	rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);
	rtc_calendar_enable(&rtc_instance);
}


//___ F U N C T I O N S ______________________________________________________

void aclock_init( aclock_state_t start_state ){
    struct rtc_calendar_time time;

    rtc_calendar_get_time_defaults(&time);
    time.year   = start_state.year;
    time.month  = start_state.month;
    time.day    = start_state.day;
    time.hour   = start_state.hour;
    time.minute = start_state.minute;
    time.second = start_state.second;

    configure_rtc_calendar();

    rtc_calendar_set_time(&rtc_instance, &time);
}


// vim: shiftwidth=2
