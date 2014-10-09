/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include "leds.h"
#include "display.h"

void configure_input(void);

void configure_input(void) {

    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_NONE;
    port_pin_set_config(PIN_PA31, &pin_conf);
    port_pin_set_config(PIN_PA30, &pin_conf);


}


int main (void)
{
    int btn_down = false;
    int click_count = 0;
    system_init();
    delay_init();
    led_init();

    /***** IMPORTANT *****/
    /* Wait a bit before configuring any thing that uses SWD
     * pins as GPIO since this can brick the device */
    /* Show a startup LED ring during the wait period */

    display_swirl(25, 800, 4 );

    configure_input();


    while (1) {
        if (port_pin_get_input_level(PIN_PA31) &&
            port_pin_get_input_level(PIN_PA30)) {
            led_enable(click_count);
            delay_us(20);
            led_disable(click_count);
            delay_ms(300);

            if(!btn_down) {
                btn_down = true;
                click_count++;
            }
            continue;
        }

        btn_down = false;

        display_swirl(25, 800, 1 );

    }

}
