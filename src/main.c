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

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define BUTTON_PIN          PIN_PA31
#define BUTTON_PIN_EIC      PIN_PA31A_EIC_EXTINT11
#define BUTTON_PIN_EIC_MUX  MUX_PA31A_EIC_EXTINT11
#define BUTTON_EIC_CHAN     11
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

void button_extint_cb( void );
  /* @brief interrupt callback for button value changes
   * @param None
   * @retrn None
   */

static void configure_extint(void);
  /* @brief enable external interrupts
   * @param None
   * @retrn None
   */

//___ V A R I A B L E S ______________________________________________________
static bool btn_extint = false;
static bool btn_down = false;

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

void button_extint_cb( void ) {
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

        extint_register_callback(button_extint_cb,
                    BUTTON_EIC_CHAN,
                    EXTINT_CALLBACK_TYPE_DETECT);

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
    system_sleep();

}

void wakeup (void) {
    extint_chan_disable_callback(BUTTON_EIC_CHAN,
                    EXTINT_CALLBACK_TYPE_DETECT);

    system_interrupt_enable_global();
    led_controller_enable();
    aclock_enable();
}

//___ F U N C T I O N S ______________________________________________________

int main (void)
{
    int16_t i = 0;
    uint8_t hour, minute, second;
    uint8_t hour_prev, minute_prev, second_prev;
    int click_count = 0;
    system_init();
    delay_init();
    aclock_init();
    led_controller_init();
    led_controller_enable();

    system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY);
    //system_set_sleepmode(SYSTEM_SLEEPMODE_IDLE_0);

    /* Show a startup LED swirl */
    display_swirl(15, 300, 4 );

    /* get intial time */
    aclock_get_time(&hour, &minute, &second);

    configure_input();
    system_interrupt_enable_global();

    enter_sleep();

    wakeup();

    while (1) {;

        int led;

        //if (port_pin_get_input_level(PIN_PA30) &&

            if (port_pin_get_input_level(BUTTON_PIN)) {
                /* button is up */
                if (btn_down) {
                    /* Just released */
                    btn_down = false;
                    display_swirl(15, 100, 2 );
                    enter_sleep();
                    wakeup();
                    }
                }
            else {
                if(!btn_down) {
                    btn_down = true;
                }
                //led_off(click_count % 60);
                //click_count++;
                //led_on(click_count % 60);
                //led_set_intensity(click_count % 60, 4);
                //led_set_blink(click_count % 60, 4);
            }


        //led_set_state(45, 8, 8);
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
        if (second != second_prev)
            led_off(second_prev);


        led_on((hour%12)*5);
        led_set_intensity(minute, 6);
        led_set_intensity(second, 1);
    }

}
