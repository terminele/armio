/**
 * \file main.c
 *
 * \brief main application entry point for armio
 *
 */

#include <asf.h>
#include <leds.h>

int main (void)
{
    int i = 0;
    system_init();
    led_init();
    delay_init();

    while (1) {
        int d;
        for (d=0; d < 5; d++) {
            int j;
            for (j=0; j < 30; j++) {

                led_enable((i+j) % 60);
                delay_us(j*3);
                led_disable((i+j) % 60);
            }
            delay_us(20);
        }

        i++;

        i = i % 60;
    }

}
