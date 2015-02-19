/** file:       control.c
  * created:    2015-01-13 17:22:15
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "control.h"
#include "aclock.h"
#include "anim.h"
#include "accel.h"
#include "display.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define CLOCK_MODE_SLEEP_TIMEOUT_TICKS     MS_IN_TICKS(7000)
#define DEFAULT_MODE_TRANS_CHK(ev_flags) \
        (   ev_flags & EV_FLAG_LONG_BTN_PRESS || \
            ev_flags & EV_FLAG_ACCEL_DCLICK_X \
        )

/* corresponding led position for the given hour */
#define HOUR_POS(hour) ((hour % 12) * 5)

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

bool clock_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for clock mode
   * @param event flags
   * @retrn flag indicating mode finish
   */
bool light_sense_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for light sensor display mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool accel_mode_tic ( event_flags_t event_flags  );
  /* @brief main tick callback for accel mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool vbatt_sense_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for vbatt sensor display mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool tick_counter_mode_tic ( event_flags_t event_flags );
  /* @brief display RTC seconds and main timer seconds
   * @param event flags
   * @retrn true on finish
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
#if VBATT_MODE
    {
        .init_cb = NULL,
        .tic_cb = vbatt_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(5000),
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    },
#endif
#if PHOTO_DEBUG
    {
        .init_cb = NULL,
        .tic_cb = light_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(10000),
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    },
#endif
#if ACCEL_DEBUG
    {
        .init_cb = NULL,
        .tic_cb = accel_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(60000),
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    },
#endif
#if TICK_DEBUG
    {
        .init_cb = NULL,
        .tic_cb = tick_counter_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(60000),
        .about_to_sleep_cb = NULL,
        .on_wakeup_cb = NULL,
    },
#endif

};

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________


uint8_t adc_light_value_scale ( uint16_t value ) {
    /* Assumes 12-bit adc read */
    if (value >= 2048)
        return (55 + (value >> 8)) % 60;

    if (value >= 1024)
        return (47 + (value >> 7)) % 60;

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

uint8_t adc_vbatt_value_scale ( uint16_t value ) {
    /* Full */
    if (value >= 3072) //> 3V --> 3/4*4096
        return 59;

    /* Greater than 1/2 full */
    if (value >= 2965) // ~2.9V --> 2.9/4*4096
        return 30 + 29*(value - 2965)/(3072 - 2965);

    /* between 1/4 and 1/2 full */
    if (value >= 2865) // ~2.8V
        return 15 + 14*(value - 2865)/(2965 - 2865);

    /* between 1/8 and 1/4 full */
    if (value >= 2765) // ~2.75
        return 7 + 7*(value - 2765)/(2865 - 2765);


    if (value <= 2048) // < 2V
        return 1;

    return 1 + 7*(value - 2048)/(2765 - 2048);


}

bool accel_mode_tic ( event_flags_t event_flags  ) {
    static display_comp_t *disp_x = NULL;
    static display_comp_t *disp_y = NULL;
    static display_comp_t *disp_z = NULL;
    static animation_t *anim_ptr = NULL;
    int16_t x,y,z;
    static uint32_t last_update_ms = 0;
#ifdef LOG_ACCEL
    static uint8_t click_cnt = 0;
    uint32_t log_data;
#endif

    if (last_update_ms == 0) {
        //accel_disable_interrupt();
    }

    //if (main_get_systime_ms() - last_update_ms < 10) {
    //    return false;
    //}

    if (anim_ptr && !anim_is_finished(anim_ptr)) {

        return false;
    }

    if (!disp_x) {
        disp_x = display_line(20, BRIGHT_MED_LOW, 1);
        disp_y = display_line(40, BRIGHT_MED_LOW, 1);
        disp_z = display_line(0, BRIGHT_MED_LOW, 1);
    }

    x = y = z = 0;
    if (!accel_data_read(&x, &y, &z)) {
        if (anim_ptr) {
            anim_release(anim_ptr);
        }
        anim_ptr = anim_swirl(0, 5, MS_IN_TICKS(16), 60, false);
        return false;
    }

#ifdef LOG_ACCEL
    /* log x and z values for now with timestamp */
    log_data = last_update_ms << 16 \
            | (((int8_t) x) << 8 & 0x0000ffff) \
            | (((int8_t) z) & 0x0000ffff);
    if (log_data == 0xffffffff) {
        log_data = 0xfffffffe;
    }
    main_log_data((uint8_t *)&log_data, sizeof(log_data), false);
#endif

    if (event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        display_comp_hide_all();
        display_comp_release(disp_x);
        display_comp_release(disp_y);
        display_comp_release(disp_z);
        disp_x = disp_y = disp_z = NULL;
        if (anim_ptr) {
            anim_release(anim_ptr);
            anim_ptr = NULL;
        }
        return true;
    }

    /* Scale values  */
    x >>= 3;
    y >>= 3;
    z >>= 3;

    display_relative( disp_x, 20, x );
    display_relative( disp_y, 40, y );
    display_relative( disp_z, 0, z );

#ifdef NOT_NOW

    if ( event_flags & EV_FLAG_ACCEL_SCLICK_X) {
        anim_cutout(display_point(15, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);

        click_cnt++;
        anim_cutout(display_point(click_cnt % 60, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);
    }

    if (event_flags & EV_FLAG_ACCEL_SCLICK_Y) {
        anim_cutout(display_point(45, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);
    }

    if (event_flags & EV_FLAG_ACCEL_SCLICK_Z) {
        anim_cutout(display_point(0, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);
    }

    if (event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        anim_cutout(display_point(30, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);

        return true;
    }

    if (event_flags & EV_FLAG_ACCEL_DCLICK_Y) {
        anim_cutout(display_point(50, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);
    }

    if (event_flags & EV_FLAG_ACCEL_DCLICK_Z) {
        anim_cutout(display_point(5, BRIGHT_MED_LOW), MS_IN_TICKS(1000),
                true);

    }
#endif

    return false;
}

bool light_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val;
    static uint32_t tick_count = 0;

    if (main_get_current_sensor() != sensor_light)
        main_set_current_sensor(sensor_light);

    if (event_flags & EV_FLAG_LONG_BTN_PRESS ||
        event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        tick_count = 0;
        return true; //transition on long presses
    }

    if (!adc_pt) {
        adc_pt = display_point(0, BRIGHT_DEFAULT);
    }


    main_start_sensor_read();

    adc_val = main_read_current_sensor(false);
    display_comp_update_pos(adc_pt, adc_light_value_scale(adc_val));

    tick_count++;

    return false;
}

bool vbatt_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val;
    static uint32_t tick_count = 0;

    /* FIXME - don't actually read vbatt sensor
     * in this mode.  VBATT should only be sampled
     * after long sleep periods */
    if (main_get_current_sensor() != sensor_vbatt)
        main_set_current_sensor(sensor_vbatt);

    if (event_flags & EV_FLAG_LONG_BTN_PRESS ||
        event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        tick_count = 0;
        return true; //transition on long presses
    }

    if (!adc_pt) {
        adc_pt = display_point(0, 3);
    }


    if (tick_count % 1000 == 999)
        main_start_sensor_read();

    adc_val = main_read_current_sensor(false);
//    if (adc_val <= 2048)
//        adc_val;
//    else
//        adc_val = adc_val - 2048;

    display_comp_update_pos(adc_pt,
            adc_vbatt_value_scale(adc_val) % 60);

    tick_count++;

    return false;
}
bool clock_mode_tic ( event_flags_t event_flags ) {
    uint8_t hour = 0, minute = 0, second = 0;
    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;


    if (event_flags & EV_FLAG_LONG_BTN_PRESS ||
        event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        if (sec_disp_ptr) {
            display_comp_release(sec_disp_ptr);
            display_comp_release(min_disp_ptr);
            display_comp_release(hour_disp_ptr);
            sec_disp_ptr = min_disp_ptr = hour_disp_ptr = NULL;
        }
        return true; //transition on long presses
    }

    /* Get latest time */
    aclock_get_time(&hour, &minute, &second);

    if (!sec_disp_ptr)
        sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);

    if (!min_disp_ptr)
        min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);

    if (!hour_disp_ptr)
        hour_disp_ptr = display_point(HOUR_POS(hour), MAX_BRIGHT_VAL);


    display_comp_update_pos(sec_disp_ptr, second);
    display_comp_update_pos(min_disp_ptr, minute);
    display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour));

    return false;
}

bool tick_counter_mode_tic ( event_flags_t event_flags ) {
    /* This mode displays RTC second value and
     * the second value based on the main timer.  This
     * is useful to see if are timer interval is too small
     * (i.e. we cant finish a main loop tick in a single
     * timer interval )*/

    /* Get latest time */
    uint8_t hour = 0, minute = 0, second = 0;
    static uint32_t ticks = 0;
    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *tick_sec_disp_ptr = NULL;

    aclock_get_time(&hour, &minute, &second);


    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {

        if (sec_disp_ptr) {
            display_comp_release(sec_disp_ptr);
            sec_disp_ptr =  NULL;
        }

        if (tick_sec_disp_ptr) {
            display_comp_release(tick_sec_disp_ptr);
            tick_sec_disp_ptr =  NULL;
        }
        return true; //transition on long presses
    }

    if (!sec_disp_ptr)
        sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);

    if (!tick_sec_disp_ptr)
        tick_sec_disp_ptr = display_point(0, BRIGHT_DEFAULT);


    ticks++;

    display_comp_update_pos(sec_disp_ptr, second);
    display_comp_update_pos(tick_sec_disp_ptr, (TICKS_IN_MS(ticks)/1000) % 60);

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


uint8_t control_mode_count( void ) {
    return sizeof(control_modes)/sizeof(control_modes[0]);
}


void control_init( void ) {

    control_mode_active = control_modes;
}
