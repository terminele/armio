/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include "leds.h"
#include "anim.h"
#include "display.h"
#include "aclock.h"
#include "accel.h"
#include "main.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define BUTTON_PIN          PIN_PA31
#define BUTTON_PIN_EIC      PIN_PA31A_EIC_EXTINT11
#define BUTTON_PIN_EIC_MUX  MUX_PA31A_EIC_EXTINT11
#define BUTTON_EIC_CHAN     11

#define BUTTON_UP           true
#define BUTTON_DOWN         false

#define LIGHT_BATT_ENABLE_PIN       PIN_PA30
#define BATT_ADC_PIN                PIN_PA02
#define LIGHT_ADC_PIN               PIN_PA03


#define MAIN_TIMER  TC5

#define MAIN_TIMER_TICK_US      1000
#define SLEEP_TIMEOUT_TICKS     7000

/* tick count before considering a button press "long" */
#define LONG_PRESS_TICKS    2000

/* max tick count between successive quick taps */
#define QUICK_TAP_INTERVAL_TICKS    500

/* corresponding led position for the given hour */
#define HOUR_POS(hour) ((hour % 12) * 5)

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

typedef enum ctl_state_t {
    STARTUP = 0,
    RUNNING,
    ENTERING_SLEEP,
    MODE_TRANSITION
} ctl_state_t;

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
void configure_input( void );
  /* @brief configure input buttons
   * @param None
   * @retrn None
   */

void setup_clock_pin_outputs( void );
  /* @brief multiplex clocks onto output pins
   * @param None
   * @retrn None
   */

static void configure_extint(void);
  /* @brief enable external interrupts
   * @param None
   * @retrn None
   */
event_flags_t get_button_event_flags( void );
  /* @brief check current button event flags
   * @param None
   * @retrn button event flags
   */
void enter_sleep( void );
  /* @brief enter into standby sleep
   * @param None
   * @retrn None
   */
void wakeup( void );
  /* @brief wakeup from sleep (e.g. enable sleeping modules)
   * @param None
   * @retrn None
   */

void main_tic ( void );
  /* @brief main control loop update function
   * @param None
   * @retrn None
   */

void main_init( void );
  /* @brief initialize main control module
   * @param None
   * @retrn None
   */

//___ V A R I A B L E S ______________________________________________________
static struct tc_module main_tc;


struct {

    /* Inactivity counter for sleeping.  Resets on any
     * user activity (e.g. button press)
     */
    uint32_t inactivity_ticks;

    /* Current button state */
    bool button_state;

    /* Counter for button ticks since pushed down */
    uint32_t button_hold_ticks;

    /* Count of multiple quick presses in a row */
    uint8_t tap_count;

    ctl_state_t state;

} main_globals;

static display_comp_t *second_disp_ptr = NULL;
static display_comp_t *minute_disp_ptr = NULL;
static display_comp_t *hour_disp_ptr = NULL;
static animation_t *sleep_wake_anim = NULL;
static struct adc_module adc_instance;

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________


void configure_adc(void)
{
	struct adc_config config_adc;
	adc_get_config_defaults(&config_adc);
        config_adc.reference = ADC_REFERENCE_INTVCC0;
        config_adc.positive_input = ADC_POSITIVE_INPUT_PIN1;
	adc_init(&adc_instance, ADC, &config_adc);

	adc_enable(&adc_instance);
}

void configure_input(void) {
    /* Configure our button as an input */
    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_NONE;
    port_pin_set_config(BUTTON_PIN, &pin_conf);

    /* Enable interrupts for the button */
    configure_extint();

    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_OUTPUT;
    port_pin_set_config(LIGHT_BATT_ENABLE_PIN, &pin_conf);
    port_pin_set_output_level(LIGHT_BATT_ENABLE_PIN, true);

  }

static void configure_extint(void)
{
	struct extint_chan_conf eint_chan_conf;
	extint_chan_get_config_defaults(&eint_chan_conf);

	eint_chan_conf.gpio_pin             = BUTTON_PIN_EIC;
	eint_chan_conf.gpio_pin_mux         = BUTTON_PIN_EIC_MUX;
        eint_chan_conf.gpio_pin_pull        = EXTINT_PULL_NONE;
        /* NOTE: cannot wake from standby with filter or edge detection ... */
	eint_chan_conf.detection_criteria   = EXTINT_DETECT_LOW;
	eint_chan_conf.filter_input_signal  = false;
	eint_chan_conf.wake_if_sleeping     = true;
	extint_chan_set_config(BUTTON_EIC_CHAN, &eint_chan_conf);

}
#ifdef CLOCK_OUTPUT
void setup_clock_pin_outputs( void ) {
    /* For debugging purposes, multiplex our
     * clocks onto output pins.. GCLK gens 4 and 7
     * should be enabled for this to function
     * properly
     * */

    struct system_pinmux_config pin_mux;
    system_pinmux_get_config_defaults(&pin_mux);

    /* MUX out the system clock to a I/O pin of the device */
    pin_mux.mux_position = MUX_PA10H_GCLK_IO4;
    system_pinmux_pin_set_config(PIN_PA10H_GCLK_IO4, &pin_mux);

    pin_mux.mux_position = MUX_PA23H_GCLK_IO7;
    system_pinmux_pin_set_config(PIN_PA23H_GCLK_IO7, &pin_mux);

}
#endif

void enter_sleep( void ) {

    /* Enable button callback to awake us from sleep */
    extint_chan_enable_callback(BUTTON_EIC_CHAN,
                    EXTINT_CALLBACK_TYPE_DETECT);

    led_controller_disable();
    aclock_disable();
    //accel_disable();
    tc_disable(&main_tc);

    //system_ahb_clock_clear_mask( PM_AHBMASK_HPB2 | PM_AHBMASK_DSU);

    system_sleep();

}

void wakeup (void) {

    //system_ahb_clock_set_mask( PM_AHBMASK_HPB2 | PM_AHBMASK_DSU);

    extint_chan_disable_callback(BUTTON_EIC_CHAN,
                    EXTINT_CALLBACK_TYPE_DETECT);

    system_interrupt_enable_global();
    led_controller_enable();
    aclock_enable();
    //accel_enable();
    tc_enable(&main_tc);
}

event_flags_t get_button_event_flags ( void ) {

    event_flags_t event_flags = 0;
    bool new_btn_state = port_pin_get_input_level(BUTTON_PIN);

    if (new_btn_state == BUTTON_UP &&
            main_globals.button_state == BUTTON_DOWN) {
        /* button has been released */
        main_globals.button_state = BUTTON_UP;
        if (main_globals.tap_count == 0) {
            /* End of a single button push */
            if (main_globals.button_hold_ticks  < LONG_PRESS_TICKS ) {
                event_flags |= EV_FLAG_SINGLE_BTN_PRESS_END;
            } else {
                event_flags |= EV_FLAG_LONG_BTN_PRESS_END;
            }
            main_globals.button_hold_ticks = 0;
        } else {
            /* TODO -- multi-tap support */
        }
    } else if (new_btn_state == BUTTON_DOWN &&
            main_globals.button_state == BUTTON_UP) {
        /* button has been pushed down */
        main_globals.button_state = BUTTON_DOWN;
    } else {
        /* button state has not changed */
        if (main_globals.button_state == BUTTON_DOWN) {
            main_globals.button_hold_ticks++;
            if (main_globals.button_hold_ticks > LONG_PRESS_TICKS) {
                event_flags |= EV_FLAG_LONG_BTN_PRESS;
            }
        }
    }

    return event_flags;
}


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

void light_sense_mode_tic ( event_flags_t event_flags ) {
    bool waiting = false;
    static display_comp_t *adc_pt = NULL;
    uint16_t result;

    if (!adc_pt) {
        adc_pt = display_point(0, BRIGHT_DEFAULT, BLINK_NONE);
    }

    if (!waiting) {
        adc_start_conversion(&adc_instance);
        waiting = true;
    }

    if (adc_read(&adc_instance, &result) == STATUS_OK) {
        waiting = false;
        display_comp_update_pos(adc_pt, adc_value_scale(result));//(result/68) % 60);
    }
}

void clock_mode_tic ( event_flags_t event_flags ) {
    uint8_t hour = 0, minute = 0, second = 0;
    static bool fast_inc = false;

    uint8_t hour_prev, minute_prev, second_prev;

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

    if (!second_disp_ptr)
        second_disp_ptr = display_point(second, MIN_BRIGHT_VAL, BLINK_NONE);

    if (!minute_disp_ptr)
        minute_disp_ptr = display_point(minute, BRIGHT_DEFAULT, BLINK_NONE);

    if (!hour_disp_ptr)
        hour_disp_ptr = display_point(HOUR_POS(hour), MAX_BRIGHT_VAL, BLINK_NONE);


    display_comp_update_pos(second_disp_ptr, second);
    display_comp_update_pos(minute_disp_ptr, minute);
    display_comp_update_pos(hour_disp_ptr, HOUR_POS(hour));

}

void main_tic( void ) {

    static display_comp_t* btn_press_display_point = NULL;

    event_flags_t event_flags = 0;

    main_globals.inactivity_ticks++;

    event_flags |= get_button_event_flags();

    /* Reset inactivity if any button event occurs */
    if (event_flags != EV_FLAG_NONE) main_globals.inactivity_ticks = 0;

    switch (main_globals.state) {
        case STARTUP:
            /* Stay in startup until animation is finished */
            if (anim_is_finished(sleep_wake_anim)) {
                main_globals.state = RUNNING;
                anim_release(sleep_wake_anim);
            }
            return;

        case ENTERING_SLEEP:
            /* Wait until animation is finished to sleep */
            if (anim_is_finished(sleep_wake_anim)) {
                anim_release(sleep_wake_anim);
                enter_sleep();

                wakeup();
                // FIXME -- should have a mode-specific "wakeup"
                display_comp_show_all();
                main_globals.inactivity_ticks = 0;
                main_globals.state = RUNNING;
            }
            return;
        case RUNNING:
            /* Check for inactivity timeout */
            if ( false && main_globals.inactivity_ticks > SLEEP_TIMEOUT_TICKS) {
                main_globals.state = ENTERING_SLEEP;
                // FIXME -- should have a mode-specific "about to sleep"
                display_comp_hide_all();
                sleep_wake_anim = anim_swirl(5, 4, 2, false);
                return;
            }
            /* FIXME -- multiple mode handler */
            //clock_mode_tic(event_flags);
            light_sense_mode_tic(event_flags);

            break;
    }
}

//___ F U N C T I O N S ______________________________________________________


void main_terminate_in_error ( uint8_t error_code ) {

    /* Display error code led */
    led_clear_all();

    /* blink error code indefinitely */
    while(1) {
        led_on(error_code, BRIGHT_DEFAULT);
        delay_ms(100);
        led_off(error_code);
        delay_ms(100);

    }
}

uint8_t main_get_multipress_count( void ) {
    return main_globals.tap_count;
}


uint32_t main_get_button_hold_ticks ( void ) {
    return main_globals.button_hold_ticks;
}
void main_init( void ) {

    struct tc_config config_tc;

    /* Initalize main state */
    main_globals.button_hold_ticks = 0;
    main_globals.tap_count = 0;
    main_globals.inactivity_ticks = 0;
    main_globals.button_state = BUTTON_UP;
    main_globals.state = STARTUP;

    /* Configure main timer counter */
    tc_get_config_defaults( &config_tc );
    config_tc.clock_source = GCLK_GENERATOR_0;
    config_tc.counter_size = TC_COUNTER_SIZE_16BIT;
    config_tc.clock_prescaler = TC_CLOCK_PRESCALER_DIV8; //give 1us count for 8MHz clock
    config_tc.wave_generation = TC_WAVE_GENERATION_MATCH_FREQ;
    config_tc.counter_16_bit.compare_capture_channel[0] = MAIN_TIMER_TICK_US;
    config_tc.counter_16_bit.value = 0;

    tc_init(&main_tc, MAIN_TIMER, &config_tc);
    tc_enable(&main_tc);

}

int main (void)
{

    system_init();
    system_apb_clock_clear_mask (SYSTEM_CLOCK_APB_APBB, PM_APBAMASK_WDT);
    system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);

    /* Errata 39.3.2 -- device may not wake up from
     * standby if nvm goes to sleep.  May not be necessary
     * for samd21e15/16 if revision E
     */
    //NVMCTRL->CTRLB.bit.SLEEPPRM = NVMCTRL_CTRLB_SLEEPPRM_DISABLED_Val;

    delay_init();
    main_init();
    aclock_init();
    led_controller_init();
    led_controller_enable();
    display_init();
    anim_init();

    configure_adc();
    //accel_init();



    /* Show a startup LED swirl */
    sleep_wake_anim = anim_swirl(5, 4, 2, true);

    /* get intial time */
    configure_input();
    system_interrupt_enable_global();
    while (!port_pin_get_input_level(BUTTON_PIN)) {
        //if btn down at startup, zero out time
        //and dont continue until released
        aclock_set_time(0, 0, 0);
    }

    while (1) {
        if (tc_get_status(&main_tc) & TC_STATUS_COUNT_OVERFLOW) {
            tc_clear_status(&main_tc, TC_STATUS_COUNT_OVERFLOW);
            main_tic();
            anim_tic();
            display_tic();
        }
    }

}
