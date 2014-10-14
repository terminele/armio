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


//___ F U N C T I O N S   ( P R I V A T E ) __________________________________
void configure_input(void) {

    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_NONE;
    port_pin_set_config(PIN_PA31, &pin_conf);
    port_pin_set_config(PIN_PA30, &pin_conf);

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

//___ F U N C T I O N S ______________________________________________________

int main (void)
{
    uint8_t hour, minute, second;
    uint8_t hour_prev, minute_prev, second_prev;
    system_init();
    delay_init();
    aclock_init();
    led_init();

    system_interrupt_enable_global();

    /***** IMPORTANT *****/
    /* Wait a bit before configuring any thing that uses SWD
     * pins as GPIO since this can brick the device */
    /* Show a startup LED ring during the wait period */
    //display_swirl(25, 800, 4 );
    led_enable(1);
    delay_s(1);
    led_disable(1);
    /* get intial time */
    aclock_get_time(&hour, &minute, &second);

//    configure_input();

    while (1) {
        int led;

#ifdef NOT_NOW
        int click_count = 0;
        bool btn_down = false;
        if (port_pin_get_input_level(PIN_PA31) &&
            port_pin_get_input_level(PIN_PA30)) {
            continue;
        }
        if(!btn_down) {
            btn_down = true;
            click_count++;
        }

#endif

        hour_prev = hour;
        minute_prev = minute;
        second_prev = second;
        /* Get latest time */
        aclock_get_time(&hour, &minute, &second);

        /* If time change, disable previous leds */
        /* ###TODO just implement a clear led state function? */
        if (hour != hour_prev)
            led_disable((hour_prev%12)*5);
        if (minute != minute_prev)
            led_disable(minute_prev);
        if (second != second_prev)
            led_disable(second_prev);


        led_enable((hour%12)*5);
        led_enable(minute);
        led_enable(second);

    }

}
