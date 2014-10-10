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

void configure_input(void);

void configure_input(void) {

    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_NONE;
    port_pin_set_config(PIN_PA31, &pin_conf);
    port_pin_set_config(PIN_PA30, &pin_conf);


}

void clock_tick_cb( void ) {
   led_enable(0);
   delay_us(1000);
   led_disable(0);

}

int main (void)
{
    system_init();
    delay_init();
    led_init();


    /***** IMPORTANT *****/
    /* Wait a bit before configuring any thing that uses SWD
     * pins as GPIO since this can brick the device */
    /* Show a startup LED ring during the wait period */

    display_swirl(25, 800, 4 );

    aclock_init(clock_tick_cb);
//    configure_input();


    while (1) {
        int btn_down = false;
        int click_count = 0;
        int i = 1;
        int switch_counter;
        int off_cycles = 1;
        int pulse_width_us = 1;


#ifdef NOW_NOW
        if (port_pin_get_input_level(PIN_PA31) &&
            port_pin_get_input_level(PIN_PA30)) {
            led_enable(click_count);
            delay_us(pulse_width_us);
            led_disable(click_count);
            delay_us(pulse_width_us*off_cycles);
            btn_down = false;
            continue;
        }

        if(!btn_down) {
            btn_down = true;
            click_count++;
        }
        switch_counter = 1000000; //1 second
        while(switch_counter > 0) {
            led_enable(i);
            delay_us(pulse_width_us);
            led_disable(i);
            delay_us(pulse_width_us*off_cycles);
            switch_counter -= pulse_width_us*(off_cycles+1);
        }
        off_cycles+=1;
        i+=1;
        //display_swirl(25, 800, 1 );

#endif
    }

}
