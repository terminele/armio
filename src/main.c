/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include "leds.h"
#include "display.h"
#include "aclock.h"
#include "accel.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define BUTTON_PIN          PIN_PA31
#define BUTTON_PIN_EIC      PIN_PA31A_EIC_EXTINT11
#define BUTTON_PIN_EIC_MUX  MUX_PA31A_EIC_EXTINT11
#define BUTTON_EIC_CHAN     11

#define LIGHT_BATT_ENABLE_PIN       PIN_PA30
#define BATT_ADC_PIN                PIN_PA02
#define LIGHT_ADC_PIN               PIN_PA03

#define ACCEL_INIT_ERROR    1 << 0

#define MAIN_TIMER  TC5

#define MAIN_TIMER_TICK_US      1000
#define SLEEP_TIMEOUT_TICKS     7000

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________


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

//___ V A R I A B L E S ______________________________________________________
static uint8_t error_status = 0; /* mask of error codes */
static struct tc_module main_tc;

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________


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
    port_pin_set_output_level(LIGHT_BATT_ENABLE_PIN, 0);

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

void enter_sleep( void ) {

    /* Enable button callback to awake us from sleep */
    extint_chan_enable_callback(BUTTON_EIC_CHAN,
                    EXTINT_CALLBACK_TYPE_DETECT);

    led_controller_disable();
    aclock_disable();
    accel_disable();

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
    accel_enable();
}


static void end_in_error ( void ) {

    /* Display error codes on hour hand leds */
    int i;

    for (i = 0; i < 11; i++) {
        if (error_status & (1 << i)) {
            led_on(i);
            led_set_blink(i, 5);
        }
    }

    while(1);
}

static void tick( void ) {

    static uint16_t sleep_timeout = 0;
    static uint8_t hour = 0, minute = 0, second = 0;
    uint8_t hour_prev, minute_prev, second_prev;
    static uint32_t button_down_cnt = 0;
    static uint16_t ticks_since_last_sec;
    bool fast_tick = false;


    if (port_pin_get_input_level(BUTTON_PIN)) {
        /* button is up */
        sleep_timeout++;
        button_down_cnt = 0;
        fast_tick = false;
        led_off(31);
        if ( sleep_timeout > SLEEP_TIMEOUT_TICKS) {
            /* Just released */
            display_swirl(10, 100, 2, 64 );
            enter_sleep();
            wakeup();
            sleep_timeout = 0;
            }
        }
    else {
        led_on(31);
        sleep_timeout = 0;
        button_down_cnt++;
        if ( button_down_cnt > 5000 && button_down_cnt % 800 == 0) {


            aclock_get_time(&hour, &minute, &second);
            hour_prev = hour;
            minute_prev = minute;
            second_prev = second;


            if (!fast_tick && button_down_cnt > 15000) {
                display_swirl(15, 50, 3, 64 );
                fast_tick = true;
            }

            if (fast_tick) {
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


            led_on((hour%12)*5);
            led_set_intensity((hour%12)*5, 10);
            led_set_intensity(minute, 6);
            led_set_intensity(second, 1);

        }

    }


    hour_prev = hour;
    minute_prev = minute;
    second_prev = second;
    /* Get latest time */
    aclock_get_time(&hour, &minute, &second);

    /* If time change, disable previous leds */
    if (hour != hour_prev)
        led_off((hour_prev%12)*5);
    if (minute != minute_prev)
        led_off(minute_prev);

    if (second != second_prev) {
        ticks_since_last_sec = 1;
        led_off(second_prev);
    }


    //led_set_intensity((hour%12)*5, 32);
    //led_set_blink(minute, 15);
    led_set_intensity(second, 31);

    ticks_since_last_sec++;

    if (ticks_since_last_sec < 128) {
        led_set_intensity((second - 1) % 60, 16 - ticks_since_last_sec << 3);
    } else {
        led_off((second - 1) % 60);

        if (ticks_since_last_sec > 872) {
            led_set_intensity(second + 1,  16 - (1000 - ticks_since_last_sec) << 3);
        }
    }

    led_set_intensity((hour%12)*5, MAX_BRIGHT_VAL);
    led_set_intensity(minute, 30);


}

//___ F U N C T I O N S ______________________________________________________

int main (void)
{
    struct tc_config config_tc;

    system_init();
    system_apb_clock_clear_mask (SYSTEM_CLOCK_APB_APBB, PM_APBAMASK_WDT);
    system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);

    /* Errata 39.3.2 -- device may not wake up from
     * standby if nvm goes to sleep.  May not be necessary
     * for samd21e15/16 if revision E
     */
    //NVMCTRL->CTRLB.bit.SLEEPPRM = NVMCTRL_CTRLB_SLEEPPRM_DISABLED_Val;

    delay_init();
    aclock_init();
    led_controller_init();
    display_init();

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
    tc_start_counter(&main_tc);


    if (!accel_init()) {
        //error_status |= ACCEL_INIT_ERROR;
    }

    led_controller_enable();

    if (error_status != 0) {
        end_in_error();
    }


    /* Show a startup LED swirl */
    display_swirl(10, 200, 2, 64);

    /* get intial time */
    configure_input();
    system_interrupt_enable_global();

    while (1) {
        if (tc_get_status(&main_tc) & TC_STATUS_COUNT_OVERFLOW) {
            tc_clear_status(&main_tc, TC_STATUS_COUNT_OVERFLOW);
            tick();
            display_tick();
        }
    }

}
