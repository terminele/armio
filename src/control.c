/** file:       control.c
  * created:    2015-01-13 17:22:15
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "control.h"
#include "aclock.h"
#include "anim.h"
#include "display.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define CLOCK_MODE_SLEEP_TIMEOUT_TICKS     7000

/* corresponding led position for the given hour */
#define HOUR_POS(hour) ((hour % 12) * 5)

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

bool clock_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for clock mode
   * @param event flags
   * @retrn None
   */
bool light_sense_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for light sensor display mode
   * @param event flags
   * @retrn None
   */

bool vbatt_sense_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for vbatt sensor display mode
   * @param event flags
   * @retrn None
   */

uint8_t adc_value_scale ( uint16_t value );
  /* @brief scales an adc read quasi-logarithmically
   * for displaying on led ring
   * @param 12-bit adc value
   * @retrn led index to display
   */

//___ V A R I A B L E S ______________________________________________________

control_mode_t *control_mode_active = NULL;

control_mode_t control_modes[] = {
    {
        .init_cb = NULL,
        .tic_cb = clock_mode_tic,
        .sleep_timeout_ticks = CLOCK_MODE_SLEEP_TIMEOUT_TICKS,
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    },
    {
        .init_cb = NULL,
        .tic_cb = light_sense_mode_tic,
        .sleep_timeout_ticks = 300000,
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    },
    {
        .init_cb = NULL,
        .tic_cb = vbatt_sense_mode_tic,
        .sleep_timeout_ticks = 300000,
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    }
};

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________


uint8_t adc_value_scale ( uint16_t value ) {

    if (value >= 2048)
        return (55 + (value >> 8)) % 60;

    if (value >= 1024)
        return 47 + (value >> 7);

    if (value >= 512)
        return 39 + (value >> 6);

    if (value >= 256)
        return 31 + (value >> 5);

    if (value >= 128)
        return 23 + (value >> 4);

    if (value >= 64)
        return 19 + (value >> 4);

    if (value >= 32)
        return 15 + (value >> 3);

    if (value >= 16)
        return 11 + (value >> 2);

    if (value >= 8)
        return 7 + (value >> 1);

    return value;

}

bool light_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val;
    static uint32_t tick_count = 0;

    if (tick_count == 0)
        main_set_current_sensor(sensor_light);

    if (event_flags & EV_FLAG_LONG_BTN_PRESS) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        tick_count = 0;
        return true; //transition on long presses
    }

    if (!adc_pt) {
        adc_pt = display_point(0, BRIGHT_DEFAULT, BLINK_NONE);
    }

    tick_count++;

    main_start_sensor_read();

    adc_val = main_read_current_sensor();
    display_comp_update_pos(adc_pt, adc_value_scale(adc_val));

    return false;
}

bool vbatt_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val;
    static uint32_t tick_count = 0;

    if (tick_count == 0)
        main_set_current_sensor(sensor_vbatt);

    if (event_flags & EV_FLAG_LONG_BTN_PRESS) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        tick_count = 0;
        return true; //transition on long presses
    }

    if (!adc_pt) {
        adc_pt = display_point(0, BRIGHT_DEFAULT, BLINK_NONE);
    }

    tick_count++;

    main_start_sensor_read();

    adc_val = main_read_current_sensor();
    if (adc_val <= 2048)
        adc_val;
    else
        adc_val = adc_val - 2048;

    display_comp_update_pos(adc_pt, (adc_val/34) % 60);

    return false;
}
bool clock_mode_tic ( event_flags_t event_flags ) {
    uint8_t hour = 0, minute = 0, second = 0;
    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;


    if (event_flags & EV_FLAG_LONG_BTN_PRESS) {
        if (sec_disp_ptr) {
            display_comp_release(sec_disp_ptr);
            display_comp_release(min_disp_ptr);
            display_comp_release(hour_disp_ptr);
            sec_disp_ptr = min_disp_ptr = hour_disp_ptr = NULL;
        }
        return true; //transition on long presses
    }

#ifdef NOT_NOW_TIME_UPDATE_MODE
    if (event_flags & EV_FLAG_LONG_BTN_PRESS) {
        uint32_t button_down_ticks = main_get_button_hold_ticks();
        if ( button_down_ticks > 5000 && button_down_ticks % 200 == 0) {

            aclock_get_time(&hour, &minute, &second);
            hour_prev = hour;
            minute_prev = minute;
            second_prev = second;


            if (!fast_inc && button_down_ticks > 15000) {
                anim_swirl(15, 50, 3, 64 );
                fast_inc = true;
            }

            if (fast_inc) {
                minute++;
            } else {
                second++;
                if (second > 59) {
                    second = 0;
                    minute++;
                }
            }

            if (minute > 59) {
                minute = 0;
                hour = (hour + 1) % 12;
            }

            aclock_set_time(hour, minute, second);
            /* If time change, disable previous leds */
            if (hour != hour_prev)
                led_off((hour_prev%12)*5);
            if (minute != minute_prev)
                led_off(minute_prev);
            if (second != second_prev)
                led_off(second_prev);


            led_clear_all();
            led_on((hour%12)*5, 10);
            led_on(minute, 6);
            led_on(second, 1);

        }

    }

#endif

    /* Get latest time */
    aclock_get_time(&hour, &minute, &second);

    if (!sec_disp_ptr)
        sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL, BLINK_NONE);

    if (!min_disp_ptr)
        min_disp_ptr = display_point(minute, BRIGHT_DEFAULT, BLINK_NONE);

    if (!hour_disp_ptr)
        hour_disp_ptr = display_point(HOUR_POS(hour), MAX_BRIGHT_VAL, BLINK_NONE);


    display_comp_update_pos(sec_disp_ptr, second);
    display_comp_update_pos(min_disp_ptr, minute);
    display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour));

    return false;
}

//___ F U N C T I O N S ______________________________________________________

void control_mode_next( void )  {
    if ((unsigned int)(control_mode_active - control_modes) >= \
            sizeof(control_modes)/sizeof(control_modes[0]) - 1) {
        control_mode_active = control_modes;
    } else {
        control_mode_active++;
    }
}

void control_mode_select( uint8_t mode_index ) {

    control_mode_active = control_modes + mode_index;
}

uint8_t control_mode_index( control_mode_t* mode_ptr ) {
    return mode_ptr - control_modes;
}

void control_init( void ) {

    control_mode_active = control_modes;
}
