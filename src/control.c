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

bool accel_point_mode_tic ( event_flags_t event_flags  );
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

bool event_debug_mode_tic ( event_flags_t event_flags );
  /* @brief debug display event flags as a point
   * @param event flags
   * @retrn true on finish
   */

bool anim_demo_mode_tic ( event_flags_t event_flags );
  /* @brief animation demo mode tic
   * @param event flags
   * @retrn true on finish
   */
uint8_t adc_light_value_scale ( uint16_t value );
  /* @brief scales a light adc read quasi-logarithmically
   * for displaying on led ring
   * @param 12-bit adc value
   * @retrn led index to display
   */


//___ V A R I A B L E S ______________________________________________________

enum {
    INIT = 0,
    ANIM_HOUR,
    ANIM_MIN,
    DISP_ALL,
} clock_mode_state;

control_mode_t *control_mode_active;
control_mode_t control_modes[] = {
#ifndef NO_CLOCK
    {
        .enter_cb = NULL,
        .tic_cb = clock_mode_tic,
        .sleep_timeout_ticks = CLOCK_MODE_SLEEP_TIMEOUT_TICKS,
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#if VBATT_MODE
    {
        .enter_cb = NULL,
        .tic_cb = vbatt_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(5000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#if PHOTO_DEBUG
    {
        .enter_cb = NULL,
        .tic_cb = light_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(10000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#if ACCEL_DEBUG
    {
        .enter_cb = NULL,
        .tic_cb = accel_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(60000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
    {
        .enter_cb = NULL,
        .tic_cb = accel_point_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(60000),
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
#ifdef EVENT_DEBUG_MODE
    {
        .enter_cb = NULL,
        .tic_cb = event_debug_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(20000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
#endif
#if ANIM_DEMO_MODE
    {
        .enter_cb = NULL,
        .tic_cb = anim_demo_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(60000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
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


bool event_debug_mode_tic ( event_flags_t event_flags  ) {
    static display_comp_t *disp_code;
    uint8_t pos = 0;

    if (!disp_code) {
        disp_code = display_point(0, BRIGHT_DEFAULT);
    }

    /* Convert event flag bit pos to int */
    if (!event_flags) { //&& main_get_systime_ms() - last_event > 200) {
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


bool anim_demo_mode_tic ( event_flags_t event_flags  ) {

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

    if (step && main_get_systime_ms() % 4000 != 1)
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
bool accel_mode_tic ( event_flags_t event_flags  ) {
    static display_comp_t *disp_x;
    static display_comp_t *disp_y;
    static display_comp_t *disp_z;
    static animation_t *anim_ptr;
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

    return false;
}



bool accel_point_mode_tic ( event_flags_t event_flags  ) {
    static display_comp_t *disp_pt = NULL;

    if (!disp_pt) {
        disp_pt = display_point(0, BRIGHT_DEFAULT);
        utils_spin_tracker_start(30);
    }


    if (event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        display_comp_hide_all();
        display_comp_release(disp_pt);
        disp_pt = NULL;
        utils_spin_tracker_end();
        return true;
    }


    display_comp_update_pos(disp_pt, utils_spin_tracker_update());
    return false;
}
bool light_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val = 0;
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


    if (event_flags & EV_FLAG_LONG_BTN_PRESS ||
        event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        tick_count = 0;
        return true; //transition on long presses
    }

#if VBATT_NO_AVERAGE
    /* don't actually read vbatt sensor
     * in this mode.  VBATT should only be sampled
     * after wakeup */
    if (main_get_current_sensor() != sensor_vbatt)
        main_set_current_sensor(sensor_vbatt);


    if (tick_count % 1000 == 999)
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

    tick_count++;

    return false;
}
bool clock_mode_tic ( event_flags_t event_flags ) {
    uint8_t hour = 0, minute = 0, second = 0, hour_fifths=0;
    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;
    static display_comp_t *hour_anim_ptr = NULL;


    if (event_flags & EV_FLAG_LONG_BTN_PRESS ||
        event_flags & EV_FLAG_ACCEL_DCLICK_X) {
        if (sec_disp_ptr) { display_comp_release(sec_disp_ptr);
            display_comp_release(min_disp_ptr);
            display_comp_release(hour_disp_ptr);
            sec_disp_ptr = min_disp_ptr = hour_disp_ptr = NULL;
        }
        return true; //transition
    }

    /* Get latest time */
    aclock_get_time(&hour, &minute, &second);

    hour_fifths = minute/12;

    if (!sec_disp_ptr)
        sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);

    if (!min_disp_ptr)
        min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);

    if (!hour_disp_ptr)
        hour_disp_ptr = display_snake(HOUR_POS(hour) + hour_fifths,
                MAX_BRIGHT_VAL, 1+hour_fifths);


    display_comp_update_pos(sec_disp_ptr, second);
    display_comp_update_pos(min_disp_ptr, minute);
    display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour) + hour_fifths);
    display_comp_update_length(hour_disp_ptr, 1+hour_fifths);

    return false;
}

bool tick_counter_mode_tic ( event_flags_t event_flags ) {
    /* This mode displays RTC second value and
     * the second value based on the main timer.  This
     * is useful to see if our timer interval is too small
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
