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


    system_init();
    delay_init();
    led_controller_init();
    led_controller_enable();



    while (1) {

    led_set_intensity(led, 1);
    delay_ms(50);
    led_off(led);
    led++;


    }



}
