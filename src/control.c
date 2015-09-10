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
#ifdef NO_TIMEOUT
#define CLOCK_MODE_SLEEP_TIMEOUT_TICKS                  MS_IN_TICKS(1000*60*5)
#else
#define CLOCK_MODE_SLEEP_TIMEOUT_TICKS                  MS_IN_TICKS(7000)
#endif
#define LONG_TIMEOUT_TICKS                              MS_IN_TICKS(1000*(60*3 + 43)) ;
#define TIME_SET_MODE_EDITING_SLEEP_TIMEOUT_TICKS       MS_IN_TICKS(80000)
#define TIME_SET_MODE_NOEDIT_SLEEP_TIMEOUT_TICKS        MS_IN_TICKS(8000)
#define EE_MODE_SLEEP_TIMEOUT_TICKS                     MS_IN_TICKS(8000)
#define EE_RUN_TIMEOUT_TICKS                            MS_IN_TICKS(3000)
#define EE_RUN_MIN_INTERTICK                            MS_IN_TICKS(100)

#define DEFAULT_MODE_TRANS_CHK(ev_flags) \
        (   ev_flags & EV_FLAG_LONG_BTN_PRESS || \
            DCLICK(ev_flags) || \
            ev_flags & EV_FLAG_SLEEP \
        )

/* corresponding led position for the given hour */
#define HOUR_POS(hour) (((hour) % 12) * 5)

/* Time it should take for hour animation to complete */
#define HOUR_ANIM_DUR_MS    300

/* Ticks run slow during edit mode (due to
 * accelerometer and floating point calcs) so
 * estimate a good tick timeout count */
#define EDIT_FINISH_TIMEOUT_TICKS   MS_IN_TICKS(1500)

#define CONTROL_MODE_EE     6
#define UTIL_MODE_COUNT     6

#ifndef FLICKER_MIN_MODE
  #define FLICKER_MIN_MODE false
#endif
//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

bool clock_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for clock mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool selector_mode_tic( event_flags_t event_flags);

bool time_set_mode_tic ( event_flags_t event_flags );
  /* @brief set clock time mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool util_control_main_tic ( event_flags_t event_flags );
  /* @brief main tick callback when in the start util mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool light_sense_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for light sensor display mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool accel_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for accel mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool accel_point_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for accel mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool vbatt_sense_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for vbatt sensor display mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool deep_sleep_enable_mode_tic( event_flags_t event_flags );
  /* @brief mode tic for entering deep sleep
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool tick_counter_mode_tic ( event_flags_t event_flags );
  /* @brief display RTC seconds and main timer seconds
   * @param event flags
   * @retrn true on finish
   */

bool digit_disp_mode_tic( event_flags_t event_flags );

bool char_disp_mode_tic ( event_flags_t event_flags );

bool ee_mode_tic ( event_flags_t event_flags );
  /* @brief animation demo mode tic
   * @param event flags
   * @retrn true on finish
   */


void set_ee_sleep_timeout(uint32_t timeout_ticks);

//___ V A R I A B L E S ______________________________________________________

ctrl_mode_t *ctrl_mode_active;

ctrl_mode_t control_modes[] = {
    {
        .enter_cb = NULL,
        .tic_cb = clock_mode_tic,
        .sleep_timeout_ticks = CLOCK_MODE_SLEEP_TIMEOUT_TICKS,
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
    {
        .enter_cb = NULL,
        .tic_cb = selector_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(1000000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    }, 
    {
        .enter_cb = NULL,
        .tic_cb = time_set_mode_tic,
        .sleep_timeout_ticks = TIME_SET_MODE_EDITING_SLEEP_TIMEOUT_TICKS,
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    }, 
//    {
//        .enter_cb = NULL,
//        .tic_cb = util_control_main_tic,
//        .sleep_timeout_ticks = MS_IN_TICKS(8000),
//        .about_to_sleep_cb = NULL,
//        .wakeup_cb = NULL,
//    },
    {
        .enter_cb = NULL,
        .tic_cb = accel_point_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
    {
        .enter_cb = NULL,
        .tic_cb = vbatt_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
    {
        .enter_cb = NULL,
        .tic_cb = light_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(30000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
    {
        .enter_cb = NULL,
        .tic_cb = deep_sleep_enable_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(10000),
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    },
    {
        .enter_cb = NULL,
        .tic_cb = ee_mode_tic,
        .sleep_timeout_ticks = EE_MODE_SLEEP_TIMEOUT_TICKS,
        .about_to_sleep_cb = NULL,
        .wakeup_cb = NULL,
    }
};

static uint32_t disp_vals[5];

/* Ticks since entering current mode */
static uint32_t modeticks = 0;

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

bool clock_mode_tic ( event_flags_t event_flags ) {
    uint8_t hour = 0, minute = 0, second = 0, hour_fifths=0;
    uint16_t hour_anim_tick_int;
    static enum { INIT, ANIM_HOUR_SWIRL, ANIM_HOUR_YOYO,
        ANIM_MIN, DISP_ALL } phase = INIT;

    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;
    static animation_t *anim_ptr = NULL;

    if (event_flags & EV_FLAG_SLEEP) {
      goto finish;
    }
    
    if (NCLICK(event_flags, 5) || \
        NCLICK(event_flags, 6)) {
      goto finish;
    }

    /* Get latest time */
    aclock_get_time(&hour, &minute, &second);

    hour_fifths = minute/12;
    if (hour > 12) {
      hour = hour % 12;

      if (hour == 0) {
        hour = 12;
      }
    }

    
    /* Enable seconds on double click */
    if (DCLICK(event_flags)) {
      
      control_modes[CONTROL_MODE_SHOW_TIME].sleep_timeout_ticks = LONG_TIMEOUT_TICKS;

      if (!sec_disp_ptr) {
        sec_disp_ptr = display_point(second, BRIGHT_DEFAULT);
      }
    }

    hour_anim_tick_int = MS_IN_TICKS(HOUR_ANIM_DUR_MS/(hour * 5));
    switch(phase) {
        case INIT:
#ifdef NO_TIME_ANIMATION
            min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);
            hour_disp_ptr = display_snake(HOUR_POS(hour), MAX_BRIGHT_VAL,
                      hour_fifths + 1, true);
            phase = DISP_ALL;
#else
            if (hour == 1) {
                /* For hour 1 a swirl doesnt animate, so draw a 'growing snake' */
                anim_ptr = anim_snake_grow( 0, 5, hour_anim_tick_int, false );
            } else {
                anim_ptr = anim_swirl(0, 5, hour_anim_tick_int, 5*(hour - 1), true);
            }

            phase = ANIM_HOUR_SWIRL;
#endif
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
#if FLICKER_MIN_MODE
                min_disp_ptr->on = false;
                anim_ptr = anim_flicker(min_disp_ptr,
                  MS_IN_TICKS(1500), false);
                phase = ANIM_MIN;
#else
                anim_ptr = anim_blink(min_disp_ptr, MS_IN_TICKS(150),
                        MS_IN_TICKS(1200), false);
                phase = ANIM_MIN;
#endif
            }

            break;
        case ANIM_MIN:
            if (anim_is_finished(anim_ptr)) {
                anim_release(anim_ptr);
                anim_ptr = NULL;
#ifdef SHOW_SEC_ALWAYS
                if (!sec_disp_ptr) {
                  sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);
                }
#endif

                phase = DISP_ALL;
            }
            break;

        case DISP_ALL:
            display_comp_show_all();

            /* Double click enables seconds and disables timeout */
            if (sec_disp_ptr) {
              display_comp_update_pos(sec_disp_ptr, second);
            }

            display_comp_update_pos(min_disp_ptr, minute);
            display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour));
            display_comp_update_length(hour_disp_ptr, hour_fifths + 1);
            

            break;
    }

    return false;

finish:
      anim_stop(anim_ptr);
      anim_release(anim_ptr);
      display_comp_release(hour_disp_ptr);
      display_comp_release(min_disp_ptr);
      display_comp_release(sec_disp_ptr);
      hour_disp_ptr = NULL;
      min_disp_ptr = NULL;
      sec_disp_ptr = NULL;
      anim_ptr = NULL;

      /* Reset sleep timeout to default */
      control_modes[CONTROL_MODE_SHOW_TIME].sleep_timeout_ticks = CLOCK_MODE_SLEEP_TIMEOUT_TICKS;

      phase = INIT;
    
    if (NCLICK(event_flags, 5) || \
        NCLICK(event_flags, 6)) {
        control_mode_set(CONTROL_MODE_SELECTOR);
        accel_events_clear();
      }

      return true;
}

bool selector_mode_tic( event_flags_t event_flags ) {
    static display_comp_t *selector_disp_ptr = NULL;
    static display_comp_t *all_disp_ptr = NULL;
    static enum {SELECTION, ANIM_SWIRL, ANIM_BLINK} state = SELECTION;
    
    if (accel_slow_click_cnt == 0 && modeticks > MS_IN_TICKS(3000)) {
      goto finish;
    }
    
    if (event_flags & EV_FLAG_SLEEP ||
        event_flags & EV_FLAG_ACCEL_SLOW_CLICK_END) {
      goto finish; 
    }

    if (accel_slow_click_cnt == 0) {
      /* No mode has been selected yet so just display a polygon
       * indicating that we're in selection mode */
      if (!all_disp_ptr) {
        all_disp_ptr = display_polygon(0, BRIGHT_DEFAULT, UTIL_MODE_COUNT);
      }
    }
    else {
      if (all_disp_ptr) {
        display_comp_release(all_disp_ptr);
        all_disp_ptr = NULL;
      }
    
      if (!selector_disp_ptr) {
        selector_disp_ptr = display_point(accel_slow_click_cnt, BRIGHT_DEFAULT);
      }

      display_comp_update_pos(selector_disp_ptr, 
          (accel_slow_click_cnt % UTIL_MODE_COUNT) * 60/UTIL_MODE_COUNT);

    }
#ifdef NOT_NOW
    /* Display a polygon representing the util modes */
    static display_comp_t *disp_ptr = NULL;

    if (!disp_ptr) {
      disp_ptr = display_polygon(0, BRIGHT_DEFAULT, UTIL_MODE_COUNT);
    }

    if (DEFAULT_MODE_TRANS_CHK(event_flags))  {
        display_comp_release(disp_ptr);
        disp_ptr = NULL;

        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }

          /* animate slow snake from current mode to next. */
          uint8_t mode_gap = 60/control_mode_count();
          uint8_t tail_len = 4;
          mode_trans_swirl = anim_swirl(
              mode_gap*control_mode_index(ctrl_mode_active),
              tail_len, MS_IN_TICKS(5 + 40/control_mode_count()),
              mode_gap - tail_len, true);


      if (mode_trans_swirl) {
        if(anim_is_finished(mode_trans_swirl)) {
          anim_release(mode_trans_swirl);
          mode_trans_swirl = NULL;
          control_mode_next();
          uint8_t mode_gap = 60/control_mode_count();
          mode_disp_point = display_point(
              mode_gap*(control_mode_index(ctrl_mode_active)) % 60,
              BRIGHT_DEFAULT);
          mode_trans_blink = anim_blink(mode_disp_point,
              BLINK_INT_MED, MS_IN_TICKS(800), false);
        }
      } else if (mode_trans_blink && anim_is_finished(mode_trans_blink)) {
        display_comp_release(mode_disp_point);
        mode_disp_point = NULL;
        anim_release(mode_trans_blink);
        mode_trans_blink = NULL;
        main_gs.state = RUNNING;
      }

#endif

    return false;
    
finish:
    display_comp_release(selector_disp_ptr);
    display_comp_release(all_disp_ptr);
    selector_disp_ptr = NULL;
    all_disp_ptr = NULL;
    
    if (accel_slow_click_cnt == 0) {
      control_mode_set(CONTROL_MODE_SHOW_TIME);
    } else if (accel_slow_click_cnt % UTIL_MODE_COUNT == 0) {
      control_mode_set(CONTROL_MODE_EE);
    } else {
      control_mode_set((accel_slow_click_cnt % UTIL_MODE_COUNT) + 1);
    }

    return true;
}

void set_ee_sleep_timeout(uint32_t timeout_ticks) {
  control_modes[CONTROL_MODE_EE].sleep_timeout_ticks = timeout_ticks;
}

bool ee_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t* display_comp = NULL;
    static animation_t *anim = NULL;
    static uint32_t ee_code = 0;
    static uint32_t run_cnt = 0;
    static uint32_t last_ev_tick = 0;
    uint32_t last_tic_delta = 0;
    static tic_fun_t ee_submode_tic = NULL;

    if ( event_flags & EV_FLAG_SLEEP ) {
      goto finish;
    }
    
    if (ee_submode_tic == NULL) {
      if (!display_comp) {
        /* Display ee mode start animation */
        display_comp = display_point(0, BRIGHT_DEFAULT);
        anim = anim_random(display_comp, MS_IN_TICKS(15), ANIMATION_DURATION_INF, false);
        /* initialize last tic to prevent events from being counted too early */
        last_ev_tick = modeticks;
        //display_comp = display_polygon(0, BRIGHT_DEFAULT, 3);
        //anim = anim_blink(display_comp, MS_IN_TICKS(400),
        //                ANIMATION_DURATION_INF, false);
        return false;
      }
    }

  #ifdef NOT_NOW
    last_tic_delta = modeticks - last_ev_tick;

    /* When still waiting for event run detection to end */
    if (last_tic_delta < EE_RUN_TIMEOUT_TICKS &&
        last_tic_delta > EE_RUN_MIN_INTERTICK) {
        switch (event_flags) {
            case EV_FLAG_ACCEL_SCLICK_X:
                ee_code+=(0x1 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_DCLICK_X:
                ee_code+=(0x2 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_TCLICK_X:
                ee_code+=(0x3 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_QCLICK_X:
                ee_code+=(0x4 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_5CLICK_X:
                ee_code+=(0x5 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_6CLICK_X:
                ee_code+=(0x6 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_7CLICK_X:
                ee_code+=(0x7 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_8CLICK_X:
                ee_code+=(0x8 << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;
            case EV_FLAG_ACCEL_9CLICK_X:
                ee_code+=(0xA << 4*run_cnt);
                run_cnt++;
                last_ev_tick = modeticks;
                break;

        }

        return false;
    } else if (last_tic_delta == EE_RUN_TIMEOUT_TICKS) {
        uint16_t tick_interval = ee_code > 0 ? MS_IN_TICKS(10 + 200/ee_code) : 1;
        uint32_t duration = ee_code < 120 && ee_code > 0 ? tick_interval*ee_code : 1;

        /* At end of event run detection. Start EE indicator animation */
        anim_release(anim);
        display_comp_show(display_comp);
        anim = anim_rotate(display_comp, true, tick_interval, duration);
        main_inactivity_timeout_reset();
        set_ee_sleep_timeout(MS_IN_TICKS(2000) + duration);

    } else if (last_tic_delta > EE_RUN_TIMEOUT_TICKS
            && anim && anim_is_finished(anim)) {
        /* EE code indicator has finished.  Figure out
         * corresponding ee code behavior if any */
        uint8_t pos = 0;
        uint16_t blink_int = MS_IN_TICKS(800);

        main_inactivity_timeout_reset();
        set_ee_sleep_timeout(EE_MODE_SLEEP_TIMEOUT_TICKS);
        /* Cleanup transition animation */
        display_comp_release(display_comp);
        display_comp = NULL;
        anim_release(anim);
        anim = NULL;

        /* Setup EE depending on code */
        switch (ee_code) {
            case 0x413:
            case 0x13:
                disp_vals[0] =  562951413UL;
                disp_vals[1] =  323979853UL;
                disp_vals[2] =  833462648UL;
                disp_vals[3] =  882059723UL;
                disp_vals[4] =  939617914UL;

                ee_submode_tic = digit_disp_mode_tic;
                break;
            case 0x72:
                disp_vals[0] =  281828172UL;
                disp_vals[1] =  325409548UL;
                disp_vals[2] =  747820635UL;
                disp_vals[3] =  942662531UL;
                disp_vals[4] =  907427577UL;

                ee_submode_tic = digit_disp_mode_tic;
                break;
            case 0x43:
                disp_vals[0] =  108030918UL;
                disp_vals[1] =  1802000418UL;
                disp_vals[2] =  140125UL;
                disp_vals[3] =  0;
                disp_vals[4] =  0;

                ee_submode_tic = char_disp_mode_tic;
                break;
            case 0x68:
                disp_vals[0] =  9035768;
                disp_vals[1] =  0;
                disp_vals[2] =  0;
                disp_vals[3] =  0;
                disp_vals[4] =  0;

                ee_submode_tic = digit_disp_mode_tic;
                break;
            case 0x2:
                ee_submode_tic = tick_counter_mode_tic;
                break;
            case 0x3:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 3);
                anim = anim_rotate(display_comp, true, MS_IN_TICKS(30), ANIMATION_DURATION_INF);
                break;
            case 0x4:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 4);
                anim = anim_rotate(display_comp, true, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 0x5:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 5);
                anim = anim_rotate(display_comp, false, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 0x6:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 12);
                anim = anim_rotate(display_comp, false, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 0x53:
                if (main_nvm_conf_data.rtc_freq_corr >= 0) {
                    pos = main_nvm_conf_data.rtc_freq_corr % 60;
                    blink_int = MS_IN_TICKS(800);
                } else {

                    pos = (-1*main_nvm_conf_data.rtc_freq_corr) % 60;
                    blink_int = MS_IN_TICKS(100);
                }

                display_comp = display_point(pos, BRIGHT_DEFAULT);
                anim = anim_blink(display_comp, blink_int, ANIMATION_DURATION_INF,
                        false);
                break;
            default:
                //display_comp = display_point(ee_code % 60, BRIGHT_DEFAULT);
                /* Display a blinking X */
                display_comp = display_polygon(7, BRIGHT_DEFAULT, 15);
                anim = anim_blink(display_comp, MS_IN_TICKS(500), ANIMATION_DURATION_INF, false );
                set_ee_sleep_timeout(MS_IN_TICKS(3000));
                break;
                //return true;
        }
            ee_code = 0;
    }


#endif

   if (ee_submode_tic) {
      if(ee_submode_tic(event_flags)) {
        /* submode is finished */
        goto finish;
      }
    }

    return false;

finish:

    if (anim) {
        anim_release(anim);
        anim = NULL;
    }

    if (display_comp) {
        display_comp_release(display_comp);
        display_comp = NULL;
    }

    /* Give submode tic a chance to cleanup as well */
    if (ee_submode_tic) ee_submode_tic(event_flags);

    ee_code = 0;
    run_cnt = 0;
    last_ev_tick = 0;
    ee_submode_tic = NULL;

    set_ee_sleep_timeout(EE_MODE_SLEEP_TIMEOUT_TICKS);

    control_mode_set(CONTROL_MODE_SHOW_TIME);
    return true;
}
bool accel_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *disp_x;
    static display_comp_t *disp_y;
    static display_comp_t *disp_z;
    static animation_t *anim_ptr;
    int16_t x,y,z;
    static uint32_t last_update_ms = 0;
#ifdef LOG_ACCEL
    uint32_t log_data;
#endif


    if ((event_flags & EV_FLAG_ACCEL_FAST_CLICK_END &&
        accel_fast_click_cnt > 4) ||
        event_flags & EV_FLAG_SLEEP){
        display_comp_hide_all();
        display_comp_release(disp_x);
        display_comp_release(disp_y);
        display_comp_release(disp_z);
        disp_x = disp_y = disp_z = NULL;
        if (anim_ptr) {
            anim_release(anim_ptr);
            anim_ptr = NULL;
        }
        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }

    if (main_get_waketime_ms() - last_update_ms < 5) {
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
bool accel_point_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *disp_pt = NULL;

    if (!disp_pt) {
        disp_pt = display_point(0, BRIGHT_DEFAULT);
        utils_spin_tracker_start(30);
    }


    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
        display_comp_hide_all();
        display_comp_release(disp_pt);
        disp_pt = NULL;
        utils_spin_tracker_end();
        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }


    display_comp_update_pos(disp_pt, utils_spin_tracker_update());
    return false;
}
bool light_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val = 0;

    set_ee_sleep_timeout(MS_IN_TICKS(30000));

    if (!adc_pt) {
        adc_pt = display_point(0, BRIGHT_DEFAULT);
        port_pin_set_output_level(LIGHT_SENSE_ENABLE_PIN, true);
    }

    if (main_get_current_sensor() != sensor_light) {
        main_set_current_sensor(sensor_light);
    }


    if (modeticks % 20  == 0) {
      main_start_sensor_read();
    }

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        port_pin_set_output_level(LIGHT_SENSE_ENABLE_PIN, false);
        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }



    adc_val = main_read_current_sensor(false);
    display_comp_update_pos(adc_pt, adc_light_value_scale(adc_val) % 60 );


    return false;
}
bool vbatt_sense_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *adc_pt = NULL;
    uint16_t adc_val;

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
        if (adc_pt) {
            display_comp_release(adc_pt);
            adc_pt = NULL;
        }
        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true; 
    }

    /* don't actually read vbatt sensor
     * in this mode.  VBATT should only be sampled
     * after wakeup */
    adc_val = main_get_vbatt_value();

    if (!adc_pt) {
        adc_pt = display_point(0, 3);
    }


    display_comp_update_pos(adc_pt,
            adc_vbatt_value_scale(adc_val) % 60);


    return false;
}
bool time_set_mode_tic ( event_flags_t event_flags ) {
    static uint8_t hour=0, minute = 0, new_minute_pos, second;
    static uint32_t timeout = 0;
    static bool is_editing = false;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;
    static animation_t *blink_ptr = NULL;



    if ( event_flags & EV_FLAG_SLEEP ||
         DCLICK(event_flags) || 
         (!is_editing && modeticks > TIME_SET_MODE_NOEDIT_SLEEP_TIMEOUT_TICKS )) {
      goto finish;
    }

    if (!min_disp_ptr) {
        /* Get latest time */
        aclock_get_time(&hour, &minute, &second /*DONT CARE */);
        hour_disp_ptr = display_point(HOUR_POS(hour), MAX_BRIGHT_VAL);
        min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);
    }

    if (!is_editing && !blink_ptr) {
        /* Enter edit mode on btn bress or x-axis click */
        if (event_flags & EV_FLAG_SINGLE_BTN_PRESS_END ||
            SCLICK(event_flags)) {
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
                goto finish;
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

finish:
    if (min_disp_ptr) {
        display_comp_release(min_disp_ptr);
        display_comp_release(hour_disp_ptr);
        min_disp_ptr = hour_disp_ptr = NULL;
    }
    if (blink_ptr) {
        anim_release(blink_ptr);
        blink_ptr = NULL;
    }
    is_editing = false;
    utils_spin_tracker_end();
 
    control_mode_set(CONTROL_MODE_SHOW_TIME);
    return true; 
}

bool deep_sleep_enable_mode_tic( event_flags_t event_flags ) {
    static display_comp_t *disp_ptr = NULL;
    
    if (!disp_ptr) {
      /* Display a polygon 'warning' sign */
      disp_ptr = display_polygon(10, BRIGHT_DEFAULT, 3);
    }
    
    if (DEFAULT_MODE_TRANS_CHK(event_flags))  {
        goto finish;
    }
    
    if (NCLICK(event_flags, 5) || NCLICK(event_flags, 6)) {
        main_deep_sleep_enable();
        goto finish;
    }

    return false;

finish:
    display_comp_release(disp_ptr);
    disp_ptr = NULL;

    control_mode_set(CONTROL_MODE_SHOW_TIME);
    return true;
}
bool tick_counter_mode_tic ( event_flags_t event_flags ) {
    /* This mode displays RTC second value and
     * the second value based on the main timer.  This
     * is useful to see if our timer interval is too small
     * (i.e. we cant finish a main loop tick in a single
     * timer interval )*/

    /* Get latest time */
    uint8_t hour = 0, minute = 0, second = 0;
    static display_comp_t *sec_disp_ptr = NULL;
    static display_comp_t *tick_sec_disp_ptr = NULL;

    set_ee_sleep_timeout(MS_IN_TICKS(10000));


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

        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true; 
    }

    if (!sec_disp_ptr)
        sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);

    if (!tick_sec_disp_ptr)
        tick_sec_disp_ptr = display_point(0, BRIGHT_DEFAULT);



    display_comp_update_pos(sec_disp_ptr, second);
    display_comp_update_pos(tick_sec_disp_ptr, (TICKS_IN_MS(modeticks)/1000) % 60);

    return false;
}

bool util_control_main_tic ( event_flags_t event_flags ) {

    /* Display a polygon representing the util modes */
    static display_comp_t *disp_ptr = NULL;

    if (!disp_ptr) {
      disp_ptr = display_polygon(0, BRIGHT_DEFAULT, control_mode_count());
    }

    if (DEFAULT_MODE_TRANS_CHK(event_flags))  {
        display_comp_release(disp_ptr);
        disp_ptr = NULL;

        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }

    return false;
}

bool char_disp_mode_tic ( event_flags_t event_flags ) {
#define CHARS_PER_UINT32  5UL
#define MAX_CHARS               25UL
    static display_comp_t *char_disp_ptr = NULL;
    static uint32_t char_idx = 0;
    static uint32_t val = 0;
    static uint32_t divisor = 1;
    static uint32_t last_update_tic = 0;

    set_ee_sleep_timeout(MS_IN_TICKS(20000));


    if ( event_flags & EV_FLAG_SLEEP ||
         accel_fast_click_cnt > 3) {
      goto finish;
    }

    if (modeticks - last_update_tic > 1000) {
      val = (disp_vals[char_idx/CHARS_PER_UINT32] / divisor) % 100;

      char_idx++;
      divisor*=100;
      last_update_tic = modeticks;

      if (char_idx % CHARS_PER_UINT32 == 0) {
        divisor = 1;
      }
      if (char_idx == MAX_CHARS) {
        goto finish;
      }
    }


    if (!char_disp_ptr) {
        char_disp_ptr = display_point(val, BRIGHT_DEFAULT);
    }

    display_comp_update_pos(char_disp_ptr, val);


    return false;

finish:
    if (char_disp_ptr) {
      display_comp_release(char_disp_ptr);
      char_disp_ptr = NULL;
    }

    char_idx = 0;
    divisor = 1;
    last_update_tic = 0;
    control_mode_set(CONTROL_MODE_SHOW_TIME);
    return true;
}

bool digit_disp_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t *digit_disp_ptr = NULL;
    static uint32_t digit_idx = 0;
    static uint32_t divisor = 1;
    static uint32_t last_update_tic = 0;
    static uint32_t digit = 0;
#define DIGITS_PER_UINT32   9UL
#define MAX_DIGITS          45UL

    set_ee_sleep_timeout(MS_IN_TICKS(25000));



    if ( event_flags & EV_FLAG_SLEEP ||
         accel_fast_click_cnt > 3) {
      goto finish;
    }

    if (modeticks - last_update_tic > 1000) {
      digit = (disp_vals[digit_idx/DIGITS_PER_UINT32] / divisor) % 10;
      digit_idx++;
      divisor*=10;
      last_update_tic = modeticks;

      if (digit_idx % DIGITS_PER_UINT32 == 0) {
        divisor = 1;
      }
      if (digit_idx == MAX_DIGITS) {
        goto finish;
      }
    }

    if (!digit_disp_ptr) {
        digit_disp_ptr = display_point(digit * 5, BRIGHT_DEFAULT);
    }

    display_comp_update_pos(digit_disp_ptr, digit * 5);

    return false;

finish:
    if (digit_disp_ptr) {
      display_comp_release(digit_disp_ptr);
      digit_disp_ptr = NULL;
    }

    digit_idx = 0;
    divisor = 1;
    last_update_tic = 0;
    control_mode_set(CONTROL_MODE_SHOW_TIME);
    return true;
}

//___ F U N C T I O N S ______________________________________________________

void control_mode_set( uint8_t mode_index) {
  modeticks = 0;
  if (mode_index >= control_mode_count()) {
      mode_index = 0;
  }
  ctrl_mode_active = control_modes + mode_index;
}

uint8_t control_mode_index( ctrl_mode_t* mode_ptr ) {
  return (mode_ptr - control_modes);
}

uint8_t control_mode_count( void ) {
  return sizeof(control_modes)/sizeof(ctrl_mode_t);
}

void control_tic( event_flags_t ev_flags) {
  modeticks++;
  ctrl_mode_active->tic_cb(ev_flags);
}

void control_init( void ) {
  ctrl_mode_active = control_modes;
}

