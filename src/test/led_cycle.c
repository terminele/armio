/**
 * \file led_cycle.c
 *
 * \brief test program that cycles through leds
 *
 */

#include <asf.h>
#include "leds.h"
#include "display.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define BUTTON_PIN          PIN_PA31
#define BUTTON_PIN_EIC      PIN_PA31A_EIC_EXTINT11
#define BUTTON_PIN_EIC_MUX  MUX_PA31A_EIC_EXTINT11
#define BUTTON_EIC_CHAN     11
//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

//___ F U N C T I O N S ______________________________________________________

int main (void)
{
    uint8_t led = 0;
    uint8_t intensity = 1;


    system_init();
    delay_init();
    led_controller_init();
    led_controller_enable();


    /* Show a startup LED swirl */
    display_swirl(10, 200, 3, 60 );

    while (1) {

        led_set_intensity(led, ++intensity);
        delay_ms(100);
        led_off(led);
        led++;
        led= led % 60;


    }



}
