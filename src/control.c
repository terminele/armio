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
#define CLOCK_MODE_SLEEP_TIMEOUT_TICKS                  MS_IN_TICKS(4500)
#define LONG_TIMEOUT_TICKS                              MS_IN_TICKS(1000*(60*3 + 43)) ;
#define INF_TIMEOUT_TICKS                               MS_IN_TICKS(1000*60*60*8)
#define TIME_SET_MODE_EDITING_SLEEP_TIMEOUT_TICKS       MS_IN_TICKS(80000)
#define TIME_SET_MODE_NOEDIT_SLEEP_TIMEOUT_TICKS        MS_IN_TICKS(8000)
#define SELECTOR_MODE_TIMEOUT_TICKS                     MS_IN_TICKS(8000)
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
/* Max tick interval for hour animation (so 1 and 2 o'clock are faster) */
#define MAX_HOUR_ANIM_TICKS     MS_IN_TICKS(40)

/* Blink intervals and duration for minute hand */
#define MIN_BLINK_INT    MS_IN_TICKS(175)
#define MIN_BLINK_DUR    MS_IN_TICKS(1400)

/* Blink faster and longer for low battery warning */
#define MIN_BLINK_INT_LOW_BATT    MS_IN_TICKS(100)
#define MIN_BLINK_DUR_LOW_BATT    MS_IN_TICKS(5000)

/* Ticks run slow during edit mode (due to
 * accelerometer and floating point calcs) so
 * estimate a good tick timeout count */
#define EDIT_FINISH_TIMEOUT_TICKS   MS_IN_TICKS(1500)

#define CONTROL_MODE_EE     10
#define UTIL_MODE_COUNT     8
/* Util control modes start at index 3 in control mode array (e.g. util mode 1 is
 * index 3, etc)*/
#define UTIL_CTRL_MODE(n) (n % (UTIL_MODE_COUNT + 1) + 2)

#ifndef FLICKER_MIN_MODE
  #define FLICKER_MIN_MODE false
#endif

#ifndef LOG_ACCEL_STREAM_IN_MODE_1
#define LOG_ACCEL_STREAM_IN_MODE_1 false
#endif

#ifndef DISABLE_SECONDS
#define DISABLE_SECONDS false
#endif

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

bool clock_mode_tic ( event_flags_t event_flags );
  /* @brief main tick callback for clock mode
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool selector_mode_tic( event_flags_t event_flags);
  /* @brief mode for selecting and entering advanced modes
   * @param event flags
   * @retrn flag indicating mode finish
   */

bool time_set_mode_tic ( event_flags_t event_flags );
  /* @brief set clock time mode
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

bool gesture_toggle_mode_tic ( event_flags_t event_flags );
  /* @brief mode to allow user disable gestures indefinitely
   * @param event flags
   * @retrn true on finish
   */

bool sparkle_mode_tic ( event_flags_t event_flags );
  /* @brief sparkles
   * @param event flags
   * @retrn true on finish
   */

bool swirl_mode_tic ( event_flags_t event_flags );
  /* @brief swirls
   * @param event flags
   * @retrn true on finish
   */

bool seconds_enable_toggle_mode_tic ( event_flags_t event_flags );
  /* @brief mode to allow user enable seconds always on
   * @param event flags
   * @retrn true on finish
   */

bool deep_sleep_enable_mode_tic( event_flags_t event_flags );
  /* @brief mode to allow user to enter deep sleep
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
        .tic_cb = clock_mode_tic,
        .sleep_timeout_ticks = CLOCK_MODE_SLEEP_TIMEOUT_TICKS,
    },
    {
        .tic_cb = time_set_mode_tic,
        .sleep_timeout_ticks = TIME_SET_MODE_EDITING_SLEEP_TIMEOUT_TICKS,
    },
    {
        .tic_cb = selector_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(20000),
    },
    {
        /* UTIL MODE #1 */
#if (LOG_ACCEL_STREAM_IN_MODE_1)
        .tic_cb = accel_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(1500000),
#else   /* LOG_ACCEL_STREAM_IN_MODE_1 */
        .tic_cb = sparkle_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
#endif  /* LOG_ACCEL_STREAM_IN_MODE_1 */
    },
    {
        /* UTIL MODE #2 */
        .tic_cb = swirl_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
    },
    {
        /* UTIL MODE #3 */
        .tic_cb = light_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(30000),
    },
    {
        /* UTIL MODE #4 */
        .tic_cb = vbatt_sense_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
    },
    {
        /* UTIL MODE #5 */
        .tic_cb = gesture_toggle_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
    },
    {
        /* UTIL MODE #6 */
        .tic_cb = accel_point_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(15000),
    },
//    {
//        /* UTIL MODE #5 */
//        .tic_cb = seconds_enable_toggle_mode_tic,
//        .sleep_timeout_ticks = MS_IN_TICKS(15000),
//    },
    {
        /* UTIL MODE #7 */
        .tic_cb = deep_sleep_enable_mode_tic,
        .sleep_timeout_ticks = MS_IN_TICKS(10000),
    },
    {
        /* UTIL MODE #8 */
        .tic_cb = ee_mode_tic,
        .sleep_timeout_ticks = EE_MODE_SLEEP_TIMEOUT_TICKS,
    }
};

static uint32_t disp_vals[9];

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

    if (MNCLICK(event_flags, 5, 6) || //set time mode
        MNCLICK(event_flags, 9, 12)) { //advanced modes
      goto finish;
    }

    /* Get latest time */
    hour = aclock_state.hour;
    minute = aclock_state.minute;
    second = aclock_state.second;

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

#if !(DISABLE_SECONDS)
      if (!sec_disp_ptr) {
        sec_disp_ptr = display_point(second, BRIGHT_LOW);
      }
#endif  /* DISABLE_SECONDS */
    }

    hour_anim_tick_int = MS_IN_TICKS(HOUR_ANIM_DUR_MS/(hour * 5));
    if (hour_anim_tick_int >= MAX_HOUR_ANIM_TICKS) {
        hour_anim_tick_int = MAX_HOUR_ANIM_TICKS;
    }
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
                uint16_t blink_int = MIN_BLINK_INT;
                uint16_t blink_dur = MIN_BLINK_DUR;

                anim_release(anim_ptr);
                min_disp_ptr = display_point(minute, BRIGHT_DEFAULT);
#if FLICKER_MIN_MODE
                min_disp_ptr->on = false;
                anim_ptr = anim_flicker(min_disp_ptr,
                  MS_IN_TICKS(1500), false);
                phase = ANIM_MIN;
#else
                if (main_is_low_vbatt()) {
                    /* Blink faster, longer for low batt warning */
                    blink_int = MIN_BLINK_INT_LOW_BATT;
                    blink_dur = MIN_BLINK_DUR_LOW_BATT;
                }

                anim_ptr = anim_blink(min_disp_ptr, blink_int, blink_dur, false);
                phase = ANIM_MIN;
#endif
            }

            break;
        case ANIM_MIN:
            if (anim_is_finished(anim_ptr)) {
                anim_release(anim_ptr);
                anim_ptr = NULL;
                if (main_user_data.seconds_always_on && !sec_disp_ptr) {
                  sec_disp_ptr = display_point(second, MIN_BRIGHT_VAL);
                }

                phase = DISP_ALL;
            }
            break;

        case DISP_ALL:
            /* Ensure minute led is on after blinking */
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

    if (MNCLICK(event_flags, 5, 6)) {
        control_mode_set(CONTROL_MODE_SET_TIME);
    }
    else if (MNCLICK(event_flags, 9, 12)) { //advanced modes
        control_mode_set(CONTROL_MODE_SELECTOR);
    }

    return true;
}

bool selector_mode_tic( event_flags_t event_flags ) {
    static display_comp_t *selector_disp_ptr = NULL;
    static display_comp_t *all_disp_ptr = NULL;
    static animation_t *mode_trans_blink = NULL;
    static uint8_t selected_mode = CONTROL_MODE_SHOW_TIME;

    if (accel_slow_click_cnt == 0 && modeticks > SELECTOR_MODE_TIMEOUT_TICKS) {
      selected_mode = CONTROL_MODE_SHOW_TIME;
      goto finish;
    }

    if (event_flags & EV_FLAG_SLEEP) {
      goto finish;
    }

    if (event_flags & EV_FLAG_ACCEL_NOT_VIEWABLE) {
        goto finish;
    }

    if (mode_trans_blink && anim_is_finished(mode_trans_blink)) {
        goto finish;
    }

    if (accel_slow_click_cnt == 0) {
      /* No mode has been selected yet so just display a polygon
       * indicating that we're in selection mode */
      if (!all_disp_ptr) {
        all_disp_ptr = display_polygon(0, BRIGHT_DEFAULT, 6); //12);//UTIL_MODE_COUNT);
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
          (accel_slow_click_cnt % (UTIL_MODE_COUNT + 1)) * 5);// * 60/UTIL_MODE_COUNT);

    }

    if (event_flags & EV_FLAG_ACCEL_SLOW_CLICK_END) {
        if ((accel_slow_click_cnt % (UTIL_MODE_COUNT + 1)) == 0) {
          /* There is not control mode 0, so if they end
           * there just go to control mode 1 */
          selected_mode = UTIL_CTRL_MODE(1);
        } else {
          selected_mode = UTIL_CTRL_MODE(accel_slow_click_cnt);
        }
        if (!mode_trans_blink) {
          mode_trans_blink = anim_blink(selector_disp_ptr,
              BLINK_INT_MED, MS_IN_TICKS(1200), false);
        }
    }

    return false;

finish:
    display_comp_release(selector_disp_ptr);
    display_comp_release(all_disp_ptr);
    anim_release(mode_trans_blink);
    selector_disp_ptr = NULL;
    all_disp_ptr = NULL;
    mode_trans_blink = NULL;

    control_mode_set(selected_mode);

    /* Reset selected mode to default */
    selected_mode = CONTROL_MODE_SHOW_TIME;

    return true;
}

void set_ee_sleep_timeout(uint32_t timeout_ticks) {
  control_modes[CONTROL_MODE_EE].sleep_timeout_ticks = timeout_ticks;
}


bool sparkle_mode_tic( event_flags_t event_flags ) {
    static display_comp_t* display_ptr = NULL;
    static animation_t *anim_ptr = NULL;

    if (!display_ptr) {
        display_ptr = display_point(0, BRIGHT_DEFAULT);
        anim_ptr = anim_random(display_ptr, MS_IN_TICKS(15), ANIMATION_DURATION_INF, false);
    }

    if (MNCLICK(event_flags, 4, 20)) {
        ctrl_mode_active->sleep_timeout_ticks = INF_TIMEOUT_TICKS;
    }

    if ( event_flags & EV_FLAG_SLEEP ||
        DCLICK(event_flags)) {

        if (anim_ptr) {
            anim_release(anim_ptr);
            anim_ptr = NULL;
        }

        if (display_ptr) {
            display_comp_release(display_ptr);
            display_ptr = NULL;
        }
        ctrl_mode_active->sleep_timeout_ticks = MS_IN_TICKS(15000);
        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }

    return false;
}

bool swirl_mode_tic( event_flags_t event_flags ) {
    static animation_t *anim_ptr = NULL;

    if (!anim_ptr) {
        anim_ptr = anim_swirl(0, 5, MS_IN_TICKS(16), 5000000, true);
    }

    if (MNCLICK(event_flags, 4, 20)) {
        ctrl_mode_active->sleep_timeout_ticks = INF_TIMEOUT_TICKS;
    }

    if ( event_flags & EV_FLAG_SLEEP ||
        DCLICK(event_flags)) {

        if (anim_ptr) {
            anim_release(anim_ptr);
            anim_ptr = NULL;
        }
        ctrl_mode_active->sleep_timeout_ticks = MS_IN_TICKS(15000);
        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }

    return false;
}

bool ee_mode_tic ( event_flags_t event_flags ) {
    static display_comp_t* display_comp = NULL;
    static animation_t *anim = NULL;
    static tic_fun_t ee_submode_tic = NULL;
    static enum {SELECTING, BLINKING, EE} phase = SELECTING;
    static uint8_t selection = 0;
    uint8_t pos;
    uint16_t blink_int;

    if ( event_flags & EV_FLAG_SLEEP ) {
      goto finish;
    }

    if (phase == SELECTING) {
        if (!display_comp) {
            display_comp = display_point(selection, BRIGHT_DEFAULT);
        }

        display_comp_update_pos(display_comp, accel_slow_click_cnt % 60);

        if ((event_flags & EV_FLAG_ACCEL_SLOW_CLICK_END) ||
             (accel_slow_click_cnt == 0 && modeticks > MS_IN_TICKS(3000) )) {
            selection = accel_slow_click_cnt;
            phase = BLINKING;
            if (!anim) {
                anim = anim_blink(display_comp, BLINK_INT_FAST, MS_IN_TICKS(800), false);
            }
            accel_events_clear();
            main_inactivity_timeout_reset();
        }
    }

    if (phase == BLINKING && anim_is_finished(anim)) {
        phase = EE;
        /* Cleanup transition animation */
        display_comp_release(display_comp);
        display_comp = NULL;
        anim_release(anim);
        anim = NULL;
        main_inactivity_timeout_reset();
        set_ee_sleep_timeout(EE_MODE_SLEEP_TIMEOUT_TICKS);

        /* Setup EE depending on code */
        switch (selection) {
            case 1:
                ee_submode_tic = tick_counter_mode_tic;
                set_ee_sleep_timeout(MS_IN_TICKS(300000));
                break;
            case 2:
                disp_vals[0] =  281828172UL;
                disp_vals[1] =  325409548UL;
                disp_vals[2] =  747820635UL;
                disp_vals[3] =  942662531UL;
                disp_vals[4] =  907427577UL;
                disp_vals[5] =  0;
                disp_vals[6] =  0;
                disp_vals[7] =  0;
                disp_vals[8] =  0;
                ee_submode_tic = digit_disp_mode_tic;
                break;
            case 3:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 3);
                anim = anim_rotate(display_comp, true, MS_IN_TICKS(30), ANIMATION_DURATION_INF);
                break;
            case 4:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 4);
                anim = anim_rotate(display_comp, true, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 5:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 5);
                anim = anim_rotate(display_comp, false, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 6:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 6);
                anim = anim_rotate(display_comp, false, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 7:
                disp_vals[0] = 920142106UL;
                disp_vals[1] = 513UL;
                disp_vals[2] = 0UL;
                disp_vals[3] = 0UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 8:
                disp_vals[0] = 1600050820UL;
                disp_vals[1] = 1405190518UL;
                disp_vals[2] = 2315140020UL;
                disp_vals[3] = 1212092300UL;
                disp_vals[4] = 520011200UL;
                disp_vals[5] = 5020018UL;
                disp_vals[6] = 20190116UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 9:
                disp_vals[0] =  main_nvm_data.lifetime_wakes;
                disp_vals[1] =  8999999876;
                disp_vals[2] =  main_nvm_data.lifetime_ticks;
                disp_vals[3] =  8999999876;
                disp_vals[4] =  main_nvm_data.wdt_resets;
                disp_vals[5] =  8999999876;
                disp_vals[6] =  main_nvm_data.filtered_gestures;
                disp_vals[7] =  0;
                disp_vals[8] =  0;

                ee_submode_tic = digit_disp_mode_tic;
                break;
            case 10:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 10);
                anim = anim_rotate(display_comp, false, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
            case 11:
                disp_vals[0] = 2018090820UL;
                disp_vals[1] = 1600140505UL;
                disp_vals[2] = 19090020UL;
                disp_vals[3] = 600250511UL;
                disp_vals[4] = 906001815UL;
                disp_vals[5] = 914252006UL;
                disp_vals[6] = 514UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 12:
                display_comp = display_polygon(0, BRIGHT_DEFAULT, 12);
                anim = anim_rotate(display_comp, false, MS_IN_TICKS(50), ANIMATION_DURATION_INF);
                break;
//cipher:VUNIRABFRAFRBSGVZR
            case 13:
                disp_vals[0] = 1809142122UL;
                disp_vals[1] = 118060201UL;
                disp_vals[2] = 719021806UL;
                disp_vals[3] = 182622UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 14:
                disp_vals[0] =  562951413UL;
                disp_vals[1] =  323979853UL;
                disp_vals[2] =  833462648UL;
                disp_vals[3] =  882059723UL;
                disp_vals[4] =  939617914UL;
                disp_vals[5] =  0;
                disp_vals[6] =  0;
                disp_vals[7] =  0;
                disp_vals[8] =  0;

                ee_submode_tic = digit_disp_mode_tic;
                break;
//cipher:BBB@EMXHBRL@FLJ@JTBOLRUXXZE
            case 15:
                disp_vals[0] = 500020202UL;
                disp_vals[1] = 1802082413UL;
                disp_vals[2] = 1012060012UL;
                disp_vals[3] = 1502201000UL;
                disp_vals[4] = 2424211812UL;
                disp_vals[5] = 526UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 16:
                disp_vals[0] = 1405220519UL;
                disp_vals[1] = 14050520UL;
                disp_vals[2] = 900250511UL;
                disp_vals[3] = 514150019UL;
                disp_vals[4] = 15232000UL;
                disp_vals[5] = 505180820UL;
                disp_vals[6] = 1821150600UL;
                disp_vals[7] = 522090600UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:EQGKNOI@PNPPV@FOF@AFTVLR@UKPI
            case 17:
                disp_vals[0] = 1411071705UL;
                disp_vals[1] = 1416000915UL;
                disp_vals[2] = 600221616UL;
                disp_vals[3] = 601000615UL;
                disp_vals[4] = 18122220UL;
                disp_vals[5] = 9161121UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 18:
                disp_vals[0] = 900152320UL;
                disp_vals[1] = 2505110019UL;
                disp_vals[2] = 18150600UL;
                disp_vals[3] = 2014052320UL;
                disp_vals[4] = 2205190025UL;
                disp_vals[5] = 1405UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 19:
                disp_vals[0] = 2018090820UL;
                disp_vals[1] = 900140505UL;
                disp_vals[2] = 2015180019UL;
                disp_vals[3] = 0UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:BPY@IXPZ@QJMM@J@YDBGN@ZS@ZIVVL@DOWZ@XJVL
            case 20:
                disp_vals[0] = 900251602UL;
                disp_vals[1] = 1700261624UL;
                disp_vals[2] = 1000131310UL;
                disp_vals[3] = 702042500UL;
                disp_vals[4] = 19260014UL;
                disp_vals[5] = 1222220926UL;
                disp_vals[6] = 2623150400UL;
                disp_vals[7] = 1222102400UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 21:
                disp_vals[0] =  108030918UL;
                disp_vals[1] =  1802000418UL;
                disp_vals[2] =  140125UL;
                disp_vals[3] =  0;
                disp_vals[4] =  0;
                disp_vals[5] =  0;
                disp_vals[6] =  0;
                disp_vals[7] =  0;
                disp_vals[8] =  0;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 22:
                disp_vals[0] = 1405220519UL;
                disp_vals[1] = 1100190900UL;
                disp_vals[2] = 1506002505UL;
                disp_vals[3] = 609060018UL;
                disp_vals[4] = 14050520UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 23:
                disp_vals[0] = 2215130518UL;
                disp_vals[1] = 116190005UL;
                disp_vals[2] = 900190503UL;
                disp_vals[3] = 2505110014UL;
                disp_vals[4] = 18150600UL;
                disp_vals[5] = 2520240919UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 24:
                disp_vals[0] = 2018211506UL;
                disp_vals[1] = 900140505UL;
                disp_vals[2] = 2505110019UL;
                disp_vals[3] = 18150600UL;
                disp_vals[4] = 2014052320UL;
                disp_vals[5] = 25UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:MMYTCKFHUKX
            case 25:
                disp_vals[0] = 320251313UL;
                disp_vals[1] = 1121080611UL;
                disp_vals[2] = 24UL;
                disp_vals[3] = 0UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 26:
                disp_vals[0] = 1525000502UL;
                disp_vals[1] = 2505001821UL;
                disp_vals[2] = 820001905UL;
                disp_vals[3] = 2009230005UL;
                disp_vals[4] = 19190514UL;
                disp_vals[5] = 820000615UL;
                disp_vals[6] = 1803001909UL;
                disp_vals[7] = 1201201925UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:VPNMKABVQKIFW
            case 27:
                disp_vals[0] = 1113141622UL;
                disp_vals[1] = 1117220201UL;
                disp_vals[2] = 230609UL;
                disp_vals[3] = 0UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 28:
                disp_vals[0] = 5130920UL;
                disp_vals[1] = 1001909UL;
                disp_vals[2] = 20011206UL;
                disp_vals[3] = 3180903UL;
                disp_vals[4] = 2009210000UL;
                disp_vals[5] = 1801150200UL;
                disp_vals[6] = 4UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 29:
                disp_vals[0] = 1900121209UL;
                disp_vals[1] = 11030920UL;
                disp_vals[2] = 8200923UL;
                disp_vals[3] = 18211525UL;
                disp_vals[4] = 2019091823UL;
                disp_vals[5] = 18150600UL;
                disp_vals[6] = 1508200001UL;
                disp_vals[7] = 414011921UL;
                disp_vals[8] = 19182500UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 30:
                disp_vals[0] = 5130920UL;
                disp_vals[1] = 19051507UL;
                disp_vals[2] = 1519002502UL;
                disp_vals[3] = 2315121900UL;
                disp_vals[4] = 1401002512UL;
                disp_vals[5] = 1309200004UL;
                disp_vals[6] = 1401030005UL;
                disp_vals[7] = 1519001504UL;
                disp_vals[8] = 803211300UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 31:
                disp_vals[0] = 520060906UL;
                disp_vals[1] = 1909001405UL;
                disp_vals[2] = 507092200UL;
                disp_vals[3] = 5180514UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 32:
                disp_vals[0] = 2520060906UL;
                disp_vals[1] = 514091400UL;
                disp_vals[2] = 900201600UL;
                disp_vals[3] = 2505110019UL;
                disp_vals[4] = 18150600UL;
                disp_vals[5] = 2520240919UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 33:
                disp_vals[0] = 1409220912UL;
                disp_vals[1] = 2000141500UL;
                disp_vals[2] = 1191221UL;
                disp_vals[3] = 1805130920UL;
                disp_vals[4] = 1421150300UL;
                disp_vals[5] = 19180520UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 34:
                disp_vals[0] = 2014052320UL;
                disp_vals[1] = 2205190025UL;
                disp_vals[2] = 2016001405UL;
                disp_vals[3] = 1100190900UL;
                disp_vals[4] = 1506002505UL;
                disp_vals[5] = 523200018UL;
                disp_vals[6] = 600252014UL;
                disp_vals[7] = 52209UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 35:
                disp_vals[0] = 2001051807UL;
                disp_vals[1] = 1401121600UL;
                disp_vals[2] = 507140900UL;
                disp_vals[3] = 1921150914UL;
                disp_vals[4] = 19200900UL;
                disp_vals[5] = 923190001UL;
                disp_vals[6] = 2106001919UL;
                disp_vals[7] = 14091103UL;
                disp_vals[8] = 803200123UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 36:
                disp_vals[0] = 5130920UL;
                disp_vals[1] = 2000041401UL;
                disp_vals[2] = 2300050409UL;
                disp_vals[3] = 600200901UL;
                disp_vals[4] = 1514001815UL;
                disp_vals[5] = 14011300UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 37:
                disp_vals[0] = 2008070918UL;
                disp_vals[1] = 301121600UL;
                disp_vals[2] = 1518230005UL;
                disp_vals[3] = 920000714UL;
                disp_vals[4] = 513UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 38:
                disp_vals[0] = 714151823UL;
                disp_vals[1] = 301121600UL;
                disp_vals[2] = 709180005UL;
                disp_vals[3] = 920002008UL;
                disp_vals[4] = 513UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 39:
                disp_vals[0] = 1507041401UL;
                disp_vals[1] = 409011904UL;
                disp_vals[2] = 820200512UL;
                disp_vals[3] = 502051805UL;
                disp_vals[4] = 19040512UL;
                disp_vals[5] = 2004140100UL;
                disp_vals[6] = 2305180508UL;
                disp_vals[7] = 919051805UL;
                disp_vals[8] = 252024UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 40:
                disp_vals[0] = 1507041401UL;
                disp_vals[1] = 2023011904UL;
                disp_vals[2] = 709120508UL;
                disp_vals[3] = 2000002008UL;
                disp_vals[4] = 2009200108UL;
                disp_vals[5] = 2316190123UL;
                disp_vals[6] = 1308200409UL;
                disp_vals[7] = 112210415UL;
                disp_vals[8] = 40520UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 41:
                disp_vals[0] = 920190123UL;
                disp_vals[1] = 1309200014UL;
                disp_vals[2] = 1200090005UL;
                disp_vals[3] = 1300200605UL;
                disp_vals[4] = 1315080025UL;
                disp_vals[5] = 14090005UL;
                disp_vals[6] = 718150507UL;
                disp_vals[7] = 109UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 42:
                disp_vals[0] = 2116130520UL;
                disp_vals[1] = 104050019UL;
                disp_vals[2] = 1805180024UL;
                disp_vals[3] = 1321UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:MMYTQAQFZMEENGHNOMINILYIXA@SSXCPHLVFRCKGYNQGW
            case 43:
                disp_vals[0] = 1720251313UL;
                disp_vals[1] = 1326061701UL;
                disp_vals[2] = 807140505UL;
                disp_vals[3] = 1409131514UL;
                disp_vals[4] = 2409251209UL;
                disp_vals[5] = 2419190001UL;
                disp_vals[6] = 2212081603UL;
                disp_vals[7] = 711031806UL;
                disp_vals[8] = 2307171425UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 44:
                disp_vals[0] = 104210112UL;
                disp_vals[1] = 2000181520UL;
                disp_vals[2] = 1815161305UL;
                disp_vals[3] = 301001909UL;
                disp_vals[4] = 920UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 45:
                disp_vals[0] = 1801230502UL;
                disp_vals[1] = 6150005UL;
                disp_vals[2] = 309132019UL;
                disp_vals[3] = 512051518UL;
                disp_vals[4] = 1415182003UL;
                disp_vals[5] = 190309UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 46:
                disp_vals[0] = 2014052320UL;
                disp_vals[1] = 2209060025UL;
                disp_vals[2] = 20160005UL;
                disp_vals[3] = 511001909UL;
                disp_vals[4] = 1815060025UL;
                disp_vals[5] = 2018150600UL;
                disp_vals[6] = 1808200025UL;
                disp_vals[7] = 505UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 47:
                disp_vals[0] = 5130920UL;
                disp_vals[1] = 1905110120UL;
                disp_vals[2] = 903000100UL;
                disp_vals[3] = 2005180107UL;
                disp_vals[4] = 520UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 48:
                disp_vals[0] = 18211525UL;
                disp_vals[1] = 1815140709UL;
                disp_vals[2] = 5031401UL;
                disp_vals[3] = 108000615UL;
                disp_vals[4] = 914151318UL;
                disp_vals[5] = 221030003UL;
                disp_vals[6] = 19090005UL;
                disp_vals[7] = 1415130504UL;
                disp_vals[8] = 309UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 49:
                disp_vals[0] = 908200009UL;
                disp_vals[1] = 2009001114UL;
                disp_vals[2] = 1502010019UL;
                disp_vals[3] = 920002021UL;
                disp_vals[4] = 513UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 50:
                disp_vals[0] = 820211820UL;
                disp_vals[1] = 5201514UL;
                disp_vals[2] = 820180105UL;
                disp_vals[3] = 1415190108UL;
                disp_vals[4] = 525010405UL;
                disp_vals[5] = 609140522UL;
                disp_vals[6] = 1520192009UL;
                disp_vals[7] = 920190415UL;
                disp_vals[8] = 1212UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 51:
                disp_vals[0] = 414011807UL;
                disp_vals[1] = 520190113UL;
                disp_vals[2] = 112060018UL;
                disp_vals[3] = 1503000819UL;
                disp_vals[4] = 1215182014UL;
                disp_vals[5] = 180512UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 52:
                disp_vals[0] = 1900211525UL;
                disp_vals[1] = 820000505UL;
                disp_vals[2] = 123001909UL;
                disp_vals[3] = 80320UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 53:
                disp_vals[0] = 19090820UL;
                disp_vals[1] = 803200123UL;
                disp_vals[2] = 2019150300UL;
                disp_vals[3] = 1815130019UL;
                disp_vals[4] = 108200005UL;
                disp_vals[5] = 2115250014UL;
                disp_vals[6] = 1801030018UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 54:
                disp_vals[0] = 920001514UL;
                disp_vals[1] = 512000513UL;
                disp_vals[2] = 1506002006UL;
                disp_vals[3] = 2115250018UL;
                disp_vals[4] = 0UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 55:
                disp_vals[0] = 19090820UL;
                disp_vals[1] = 2019150807UL;
                disp_vals[2] = 500061500UL;
                disp_vals[3] = 1820030512UL;
                disp_vals[4] = 2520090309UL;
                disp_vals[5] = 1223150800UL;
                disp_vals[6] = 19UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 56:
                disp_vals[0] = 820001409UL;
                disp_vals[1] = 1315030005UL;
                disp_vals[2] = 1405141516UL;
                disp_vals[3] = 615001920UL;
                disp_vals[4] = 300251300UL;
                disp_vals[5] = 921031809UL;
                disp_vals[6] = 115020020UL;
                disp_vals[7] = 106000418UL;
                disp_vals[8] = 503UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 57:
                disp_vals[0] = 2019180906UL;
                disp_vals[1] = 1405232000UL;
                disp_vals[2] = 919002520UL;
                disp_vals[3] = 108030024UL;
                disp_vals[4] = 615001918UL;
                disp_vals[5] = 2018150600UL;
                disp_vals[6] = 1808200025UL;
                disp_vals[7] = 2016000505UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 58:
                disp_vals[0] = 2520060906UL;
                disp_vals[1] = 522051900UL;
                disp_vals[2] = 19090014UL;
                disp_vals[3] = 1516211503UL;
                disp_vals[4] = 415030014UL;
                disp_vals[5] = 5UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:LHINC@RSMFQ@UNOM@QM@YJYE@SRFOIF@WW
            case 59:
                disp_vals[0] = 314090812UL;
                disp_vals[1] = 613191800UL;
                disp_vals[2] = 1514210017UL;
                disp_vals[3] = 13170013UL;
                disp_vals[4] = 5251025UL;
                disp_vals[5] = 915061819UL;
                disp_vals[6] = 23230006UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
//cipher:EEAMPHTWVPOVFCAYPCPRLGAJI
            case 60:
                disp_vals[0] = 1613010505UL;
                disp_vals[1] = 1622232008UL;
                disp_vals[2] = 103062215UL;
                disp_vals[3] = 1816031625UL;
                disp_vals[4] = 910010712UL;
                disp_vals[5] = 0UL;
                disp_vals[6] = 0UL;
                disp_vals[7] = 0UL;
                disp_vals[8] = 0UL;
                ee_submode_tic = char_disp_mode_tic;
                break;
            case 86:
                disp_vals[0] =  9035768;
                disp_vals[1] =  0;
                disp_vals[2] =  0;
                disp_vals[3] =  0;
                disp_vals[4] =  0;
                disp_vals[5] =  0;
                disp_vals[6] =  0;
                disp_vals[7] =  0;
                disp_vals[8] =  0;
                ee_submode_tic = digit_disp_mode_tic;
                break;
            default:
                /* Display a blinking hexagon */
                display_comp = display_polygon(5, BRIGHT_DEFAULT, 6);
                anim = anim_blink(display_comp, MS_IN_TICKS(500), ANIMATION_DURATION_INF, false );
                set_ee_sleep_timeout(MS_IN_TICKS(3000));
                break;
        }
            selection = 0;
    }

   if (ee_submode_tic) {
      if(ee_submode_tic(event_flags)) {
        /* submode is finished */
        ee_submode_tic = NULL;
        goto finish;
      }
    } else if (phase == EE) {
        if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
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

    ee_submode_tic = NULL;
    phase = SELECTING;

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
#if (LOG_ACCEL_STREAM_IN_MODE_1)
    uint8_t delta_time = 0;
    uint32_t log_data;
#endif  /* LOG_ACCEL_STREAM_IN_MODE_1 */


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

    if (main_get_waketime_ms() - last_update_ms < 10) {
        return false;
    }
#if (LOG_ACCEL_STREAM_IN_MODE_1)
    delta_time = main_get_waketime_ms() - last_update_ms;
#endif  /* LOG_ACCEL_STREAM_IN_MODE_1 */
    last_update_ms = main_get_waketime_ms();

    if (anim_ptr && !anim_is_finished(anim_ptr)) {

        return false;
    }

    if (!disp_x) {
     //   disp_x = display_point(20, BRIGHT_MED_LOW);//, 1);
     //   disp_y = display_point(40, BRIGHT_MED_LOW);//, 1);
     //   disp_z = display_point(0, BRIGHT_MED_LOW);//, 1);
    }

    x = y = z = 0;
    if (!accel_data_read(&x, &y, &z)) {
        if (anim_ptr) {
            anim_release(anim_ptr);
        }
        anim_ptr = anim_swirl(0, 5, MS_IN_TICKS(16), 60, false);
        return false;
    }

#if (LOG_ACCEL_STREAM_IN_MODE_1)
    /* log x and z values for now with timestamp */
    log_data = ( delta_time & 0xff ) << 24 \
            | (((int8_t) x) << 16 & 0x00ff0000) \
            | (((int8_t) y) << 8 & 0x0000ff00) \
            | (((int8_t) z) & 0x000000ff);

    if ( log_data == 0xffffffff ) {
        /* make sure we dont have an accidental 'stop' word */
        log_data = 0x00ffffff;
    }
    main_log_data((uint8_t *)&log_data, sizeof(log_data), false);
#endif  /* LOG_ACCEL_STREAM_IN_MODE_1 */


    /* Scale values  */
    x >>= 3;
    y >>= 3;
    z >>= 3;

//    display_relative( disp_x, 20, x );
//    display_relative( disp_y, 40, y );
//    display_relative( disp_z, 0, z );

    //display_comp_update_pos(disp_x, 20 + x);
    //display_comp_update_pos(disp_y, 40 + y);
    //display_comp_update_pos(disp_z, z);
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
    static uint8_t hour=0, minute = 0, new_minute_pos = 0, second = 0;
    static uint32_t timeout = 0;
    static bool is_editing = false;
    static display_comp_t *min_disp_ptr = NULL;
    static display_comp_t *hour_disp_ptr = NULL;
    static animation_t *blink_ptr = NULL;



    if ( event_flags & EV_FLAG_SLEEP ||
         DCLICK(event_flags) ||
         (!is_editing && (modeticks > TIME_SET_MODE_NOEDIT_SLEEP_TIMEOUT_TICKS ))) {
      goto finish;
    }

    if (!is_editing && !blink_ptr) {
        if (event_flags & EV_FLAG_ACCEL_NOT_VIEWABLE) {
            goto finish;
        }
    }

    if (modeticks < 300) {
        /* if this is too long you can mistakenly think that you have
         * correclty performed a 3-tap and the watch is off */
        /* Pause briefly with the display off to indicate
         * entrance into time setting mode */
        return false;
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
    
    /* Zero-out the seconds on 3 or more clicks so user
     * can set the seconds close to accurate if desired */
    if (!is_editing && MNCLICK(event_flags, 4, 10)) {
        aclock_set_time(hour, minute, 0);
        goto finish;
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

    /* Ensure main inactivity timeout is not triggered when editing */
    main_inactivity_timeout_reset();

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

static bool toggle_pref_mode_tic( event_flags_t event_flags, bool *pref_ptr ) {

    static display_comp_t *on_disp_ptr = NULL;
    static display_comp_t *off_disp_ptr = NULL;
    static int16_t x,y,z;
    static int16_t y_ctr = 0; //for duration y has been in a certain orientation
    static uint32_t last_update_ms = 0;

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {

        display_comp_release(on_disp_ptr);
        display_comp_release(off_disp_ptr);
        on_disp_ptr = off_disp_ptr = NULL;

        control_mode_set(CONTROL_MODE_SHOW_TIME);
        return true;
    }

    if (main_get_waketime_ms() - last_update_ms < 20) {
        return false;
    }

    x = y = z = 0;
    accel_data_read(&x, &y, &z);
    last_update_ms = main_get_waketime_ms();

//    if (SCLICK(event_flags)) {
//        *pref_ptr = !(*pref_ptr);
//    }


    /* Update our y orientation counter */
    if (y < -12) { //12 oclock down
        if (y_ctr > 0) {
            y_ctr=-1;
        } else {
            y_ctr--;
        }
    } else if (y > 12) { //6 oclock down
        if (y_ctr < 0) {
            y_ctr=1;
        } else {
            y_ctr++;
        }
    }

    if (y_ctr >= 10) {
        *pref_ptr = false;
    } else if (y_ctr <= -10) {
        *pref_ptr = true;
    }

    if (*pref_ptr) {
        if (off_disp_ptr) display_comp_hide(off_disp_ptr);

        if (!on_disp_ptr) {
            on_disp_ptr = display_line(59, BRIGHT_MED_LOW, 3);
        }

        display_comp_show(on_disp_ptr);

    } else {
        if (on_disp_ptr) display_comp_hide(on_disp_ptr);

        if (!off_disp_ptr) {
            off_disp_ptr = display_point(30, BRIGHT_MED_LOW);
        }

        display_comp_show(off_disp_ptr);
    }

    return false;
}

bool gesture_toggle_mode_tic( event_flags_t event_flags ) {
    return toggle_pref_mode_tic(event_flags, &main_user_data.wake_gestures);
}

bool seconds_enable_toggle_mode_tic ( event_flags_t event_flags ) {
    return toggle_pref_mode_tic(event_flags, &main_user_data.seconds_always_on);
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

    set_ee_sleep_timeout(MS_IN_TICKS(800000));


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


bool char_disp_mode_tic ( event_flags_t event_flags ) {
#define CHARS_PER_UINT32  5UL
#define MAX_CHARS               45UL
    static display_comp_t *char_disp_ptr = NULL;
    static uint32_t char_idx = 0;
    static uint32_t val = 0;
    static uint32_t divisor = 1;
    static uint32_t last_update_tic = 0;

    set_ee_sleep_timeout(MS_IN_TICKS(90000));

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
      goto finish;
    }

    if (modeticks - last_update_tic > 2000) {
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

    if (DEFAULT_MODE_TRANS_CHK(event_flags)) {
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
    
    if (modeticks - last_update_tic < 50) {
        display_comp_hide(digit_disp_ptr);
    } else {
        display_comp_show(digit_disp_ptr);
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

  accel_events_clear();
}

uint8_t control_mode_index( ctrl_mode_t* mode_ptr ) {
  return (mode_ptr - control_modes);
}

uint8_t control_mode_count( void ) {
  return sizeof(control_modes)/sizeof(ctrl_mode_t);
}

void control_tic( event_flags_t ev_flags) {
  ctrl_mode_active->tic_cb(ev_flags);
  modeticks++;
}

void control_init( void ) {
  ctrl_mode_active = control_modes;
}
