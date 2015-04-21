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
#include "utils.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define CLOCK_MODE_SLEEP_TIMEOUT_TICKS     MS_IN_TICKS(6000)
#define DEFAULT_MODE_TRANS_CHK(ev_flags) \
        (   ev_flags & EV_FLAG_LONG_BTN_PRESS || \
            ev_flags & EV_FLAG_ACCEL_DCLICK_X || \
            ev_flags & EV_FLAG_SLEEP \
        )

/* corresponding led position for the given hour */
#define HOUR_POS(hour) ((hour % 12) * 5)

/* Time it should take for hour animation to complete */
#define HOUR_ANIM_DUR_MS    300

/* Ticks run slow during edit mode (due to
 * accelerometer and floating point calcs) so
 * estimate a good tick timeout count */
#define EDIT_FINISH_TIMEOUT_TICKS MS_IN_TICKS(2000)
//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

bool clock_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief main tick callback for clock mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool time_set_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief set clock time mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool light_sense_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief main tick callback for light sensor display mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool accel_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief main tick callback for accel mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool accel_point_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief main tick callback for accel mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool vbatt_sense_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief main tick callback for vbatt sensor display mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool tick_counter_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief display RTC seconds and main timer seconds
   * @param event flags
   * @retrn true on finish
   */

bool event_debug_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief debug display event flags as a point
   * @param event flags
   * @retrn true on finish
   */

bool anim_demo_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt );
  /* @brief animation demo mode tic
   * @param event flags
   * @retrn true on finish
   */


//___ V A R I A B L E S ______________________________________________________

control_mode_t *control_mode_active;
control_mode_t control_modes[] = {
    {
        .enter_cb = NULL,
        .tic_cb = clock_mode_tic,
        .sleep_timeout_ticks = CLOCK_MODE_SLEEP_TIMEOUT_TICKS,
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#if ANIM_DEMO_MODE
    {
        .enter_cb = NULL,
        .tic_cb = anim_demo_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(30000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#if PHOTO_DEBUG
    {
        .enter_cb = NULL,
        .tic_cb = light_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(5000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
    {
        .enter_cb = NULL,
        .tic_cb = accel_point_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(30000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#if VBATT_MODE
    {
        .enter_cb = NULL,
        .tic_cb = vbatt_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(5000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
    {
        .enter_cb = NULL,
        .tic_cb = time_set_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(30000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#if ACCEL_DEBUG
    {
        .enter_cb = NULL,
        .tic_cb = accel_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(120000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#ifdef EVENT_DEBUG_MODE
    {
        .enter_cb = NULL,
        .tic_cb = event_debug_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(20000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#if TICK_DEBUG
    {
        .enter_cb = NULL,
        .tic_cb = tick_counter_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(60000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif

};

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________


bool event_debug_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    static display_comp_t *disp_code;
    uint8_t pos = 0;

    if (!disp_code) {
        disp_code = display_point(0, BRIGHT_DEFAULT);
    }

    /* Convert event flag bit pos to int */
    if (!event_flags) {
        display_comp_update_pos(disp_code, 0);
        return false;
    } else {
        display_comp_hide(disp_code);
    }


    do {
        while (!(event_flags & 0x01)) {
            event_flags >>= 1;
            pos++;
        }

        anim_cutout(display_point(pos, BRIGHT_DEFAULT), MS_IN_TICKS(500), true);
        event_flags >>= 1;
        pos++;
    } while (event_flags);

    return false;
}

bool anim_demo_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {

    static display_comp_t* display_comp = NULL;
    static animation_t *anim = NULL;
    static int8_t step = 0;
    uint8_t polycnt;

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
        if (anim) {
            anim_release(anim);
            anim = NULL;
        }

        if (display_comp) {
            display_comp_release(display_comp);
            display_comp = NULL;
        }

        step = 0;
        return true;
    }

    if (step && main_get_waketime_ms() % 4000 != 1)
        return false;

    step++;

    switch (step) {
        case 1:
            if (display_comp)
                display_comp_release(display_comp);
            if (anim)
                anim_release(anim);

            display_comp = display_point(0, BRIGHT_DEFAULT);
            anim = anim_random(display_comp, MS_IN_TICKS(10), ANIMATION_DURATION_INF);
            break;
        case 2:
            display_comp_release(display_comp);
            anim_release(anim);
            display_comp = display_line(0, BRIGHT_DEFAULT, 5);
            anim = anim_rotate(display_comp, true, MS_IN_TICKS(8), ANIMATION_DURATION_INF);
            break;
        default:
            display_comp_release(display_comp);
            anim_release(anim);
            polycnt = 3 + rand() % 10;
            display_comp = display_polygon(0, BRIGHT_DEFAULT, polycnt);
            anim = anim_rotate(display_comp, polycnt % 2, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
            step++;

            if (step > 12) {
                step = 0;
                display_comp_release(display_comp);
                anim_release(anim);
                display_comp = NULL;
                anim = NULL;
            }
    }


    return false;
}
bool accel_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    static display_comp_t *disp_x;
    static display_comp_t *disp_y;
    static display_comp_t *disp_z;
    static animation_t *anim_ptr;
    int16_t x,y,z;
    static uint32_t last_update_ms = 0;
#ifdef LOG_ACCEL
    uint32_t log_data;
#endif

    if (event_flags & EV_FLAG_ACCEL_DCLICK_X || event_flags & EV_FLAG_SLEEP) {
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

    if (main_get_waketime_ms() - last_update_ms < 10) {
        return false;
    }

    last_update_ms = main_get_waketime_ms();

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


    /* Scale values  */
    x >>= 3;
    y >>= 3;
    z >>= 3;

    display_relative( disp_x, 20, x );
    display_relative( disp_y, 40, y );
    display_relative( disp_z, 0, z );

    return false;
}

bool accel_point_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    static display_comp_t *disp_pt = NULL;

    if (!disp_pt) {
        disp_pt = display_point(0, BRIGHT_DEFAULT);
        utils_spin_tracker_start(30);
    }


    if (event_flags & EV_FLAG_ACCEL_DCLICK_X || event_flags & EV_FLAG_SLEEP) {
        display_comp_hide_all();
        display_comp_release(disp_pt);
        disp_pt = NULL;
        utils_spin_tracker_end();
        return true;
    }


    display_comp_update_pos(disp_pt, utils_spin_tracker_update());
    return false;
}

bool light_sense_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val = 0;

    if (main_get_current_sensor() != sensor_light)
        main_set_current_sensor(sensor_light);

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        return true; //transition on long presses
    }

    if (!adc_pt) {
        adc_pt = display_point(0, BRIGHT_DEFAULT);
    }


    main_start_sensor_read();

    adc_val = main_read_current_sensor(false);
    display_comp_update_pos(adc_pt, adc_light_value_scale(adc_val));


    return false;
}

bool vbatt_sense_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val;


    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        return true; //transition on long presses
    }

#if VBATT_NO_AVERAGE
    /* don't actually read vbatt sensor
     * in this mode.  VBATT should only be sampled
     * after wakeup */
    if (main_get_current_sensor() != sensor_vbatt)
        main_set_current_sensor(sensor_vbatt);


    if (tick_cnt % 1000 == 999)
        main_start_sensor_read();

    adc_val = main_read_current_sensor(false);
#else
    adc_val = main_get_vbatt_value();
#endif

    if (!adc_pt) {
        adc_pt = display_point(0, 3);
    }


    display_comp_update_pos(adc_pt,
            adc_vbatt_value_scale(adc_val) % 60);


    return false;
}
bool clock_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    uint8_t hour = 0, minute = 0, second = 0, hour_fifths=0;
    uint16_t hour_anim_tick_int;
    static enum { INIT, ANIM_HOUR_SWIRL, ANIM_HOUR_YOYO,
        ANIM_MIN, DISP_ALL } phase = INIT;

    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;
    static animation_t *anim_ptr = NULL;



    if (event_flags & EV_FLAG_LONG_BTN_PRESS ||
        event_flags & EV_FLAG_ACCEL_TCLICK_X ||
        event_flags & EV_FLAG_SLEEP) {

        anim_stop(anim_ptr);
        anim_release(anim_ptr);
        display_comp_release(hour_disp_ptr);
        display_comp_release(min_disp_ptr);
        display_comp_release(sec_disp_ptr);
        hour_disp_ptr = NULL;
        min_disp_ptr = NULL;
        sec_disp_ptr = NULL;
        anim_ptr = NULL;

        phase = INIT;

        return true; //transition
    }

    /* Get latest time */
    aclock_get_time(&hour, &minute, &second);

    hour_fifths = minute/12;
    hour_anim_tick_int = MS_IN_TICKS(HOUR_ANIM_DUR_MS/(hour * 5));
    switch(phase) {
        case INIT:


            if (hour == 1) {
                /* For hour 1 a swirl doesnt animate, so draw a 'growing snake' */
                anim_ptr = anim_snake_grow( 0, 5, hour_anim_tick_int, false );
            } else {
                anim_ptr = anim_swirl(0, 5, hour_anim_tick_int, 5*(hour - 1), true);
            }

            phase = ANIM_HOUR_SWIRL;
            break;
        case ANIM_HOUR_SWIRL:

            if (anim_is_finished(anim_ptr)) {
                anim_release(anim_ptr);
                hour_disp_ptr = display_snake(HOUR_POS(hour), MAX_BRIGHT_VAL,
                        hour_fifths + 1, true);

                anim_ptr = anim_yoyo(hour_disp_ptr, hour_fifths + 1,
                        hour_anim_tick_int, 1, false);
                phase = ANIM_HOUR_YOYO;
            }
            break;
        case ANIM_HOUR_YOYO:
            if (anim_is_finished(anim_ptr)) {
                anim_release(anim_ptr);
                min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);
                anim_ptr = anim_blink(min_disp_ptr, MS_IN_TICKS(400),
                        MS_IN_TICKS(2000), false);

                phase = ANIM_MIN;
            }

            break;
        case ANIM_MIN:
            if (anim_is_finished(anim_ptr)) {
                anim_release(anim_ptr);
                anim_ptr = NULL;

                sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);

                phase = DISP_ALL;
            }
            break;

        case DISP_ALL:

            display_comp_update_pos(sec_disp_ptr, second);
            display_comp_update_pos(min_disp_ptr, minute);

            display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour));
            display_comp_update_length(hour_disp_ptr, hour_fifths + 1);

            break;
    }

    //anim_update_length(hour_anim_ptr, hour_fifths + 1);

    return false;
}

bool time_set_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    static uint8_t hour=0, minute = 0, new_minute_pos, second;
    static uint32_t timeout = 0;
    static bool is_editing = false;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;
    static animation_t *blink_ptr = NULL;


    if (!is_editing && DEFAULT_MODE_TRANS_CHK(event_flags)){
        if (min_disp_ptr) {
            display_comp_release(min_disp_ptr);
            display_comp_release(hour_disp_ptr);
            min_disp_ptr = hour_disp_ptr = NULL;
        }
        if (blink_ptr) {
            anim_release(blink_ptr);
            blink_ptr = NULL;
        }
        utils_spin_tracker_end();
        return true; //transition
    }



    if (!min_disp_ptr) {
        /* Get latest time */
        aclock_get_time(&hour, &minute, &second /*DONT CARE */);
        min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);
        hour_disp_ptr = display_point(HOUR_POS(hour), MAX_BRIGHT_VAL);
    }

    if (!is_editing && !blink_ptr) {
        /* Enter edit mode on btn bress or z-axis double-click */
        if (event_flags & EV_FLAG_SINGLE_BTN_PRESS_END ||
            event_flags & EV_FLAG_ACCEL_SCLICK_Z) {
                blink_ptr = anim_blink(min_disp_ptr, BLINK_INT_FAST,
                        MS_IN_TICKS(1200), false);
        }
    }


    if (blink_ptr) {
        if (anim_is_finished(blink_ptr)) {
            anim_release(blink_ptr);
            blink_ptr = NULL;

            display_comp_show(min_disp_ptr);
            if (!is_editing) {
                /* Entering edit mode */
                utils_spin_tracker_start( minute );
                is_editing = true;
            } else {
                /* Exiting edit mode  Set updated time */
                is_editing = false;
                aclock_set_time(hour, minute, 0);
            }
        }
        return false; //nothing should be done during animations
    }

    if (!is_editing) return false;

    new_minute_pos = utils_spin_tracker_update();

    if (new_minute_pos == minute) {
        /* minute position has not changed.  Increase
         * timeout counter */
        timeout++;

        if (timeout > EDIT_FINISH_TIMEOUT_TICKS) {

            /* Start Finishing animation */
            blink_ptr = anim_blink(min_disp_ptr, BLINK_INT_MED,
                    MS_IN_TICKS(2000), false);
            timeout = 0;
            return false;
        }
    } else {

        timeout = 0;
        /* Check if hour should be incremented or decremented */
        if (new_minute_pos < minute) {
            if ( minute - new_minute_pos > 50) {
                ///### assumes tracker pos wont update more
                //than 10 positions in one tick
                hour++;
                if (hour > 12)
                    hour = 1;
            }
        } else {
            if ( new_minute_pos - minute > 50) {
                ///### assumes tracker pos wont update more
                //than 10 positions in one tick
                hour--;
                if (hour == 0)
                    hour = 12;
            }
        }

        minute = new_minute_pos;

        display_comp_update_pos(min_disp_ptr, minute);
        display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour));
    }


    return false;
}

bool tick_counter_mode_tic ( event_flags_t event_flags, uint32_t tick_cnt ) {
    /* This mode displays RTC second value and
     * the second value based on the main timer.  This
     * is useful to see if our timer interval is too small
     * (i.e. we cant finish a main loop tick in a single
     * timer interval )*/

    /* Get latest time */
    uint8_t hour = 0, minute = 0, second = 0;
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



    display_comp_update_pos(sec_disp_ptr, second);
    display_comp_update_pos(tick_sec_disp_ptr, (TICKS_IN_MS(tick_cnt)/1000) % 60);

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
