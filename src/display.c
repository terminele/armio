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
    int i = -tail_len;
    while(revolutions) {
        for (i=0; i < 60; i++) {
            int advance_countdown = tick_us;
            while(advance_countdown > 0) {
                int j = 0;
                while( j < tail_len && advance_countdown > 0 ) {
                    int ontime_us = j*MIN_DUTY_CYCLE_US;
                    led_enable((i+j) % 60);
                    delay_us(ontime_us);
                    led_disable((i+j) % 60);
                    delay_us(ontime_us);
                    advance_countdown-=ontime_us*2;
                    j++;
                }

                delay_us(20);
                advance_countdown-=20;
            }
        }

    revolutions--;
    }


}

void display_init(void) {


}
