/**
 * \file display.c
 *
 * \brief display controller
 *
 *  higher level control functions of the leds
 *  e.g. animated patterns, flashes, etc
 *
 */

#include <asf.h>
#include <leds.h>
#include <display.h>

#define MIN_DUTY_CYCLE_US 1

void display_swirl(int tail_len, int tick_us, int revolutions) {
    while(revolutions) {
        for (int i=0; i < 60; i++) {
            /* light up current swirl segment */
            for ( int j = 0; j  < tail_len; j++ ) {
                uint8_t intensity = 3 + j*(1 << BRIGHT_RES)/tail_len;
                led_set_intensity((i+j) % 60, intensity);
            }
            delay_us(tick_us);
            led_disable(i);
        }

    led_disable_all();
    revolutions--;
    }


}

void display_init(void) {


}
