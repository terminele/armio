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

#define SEGMENT_PIN_PORT_MASK ( \
      1 << PIN_PA16	|	\
      1 << PIN_PA15	|	\
      1 << PIN_PA14	|	\
      1 << PIN_PA11	|	\
      1 << PIN_PA07	|	\
      1 << PIN_PA06	|	\
      1 << PIN_PA05	|	\
      1 << PIN_PA04	|	\
      1 << PIN_PA28	|	\
      1 << PIN_PA27	|	\
      1 << PIN_PA22	|	\
      1 << PIN_PA19 )

#define BANK_PIN_PORT_MASK (    \
      1 << PIN_PA17     |       \
      1 << PIN_PA18     |       \
      1 << PIN_PA25     |       \
      1 << PIN_PA24     |       \
      1 << PIN_PA23 )

  struct port_config pin_conf;

    /* Configure bank and segment pin groups as outputs */
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_OUTPUT;

    port_group_set_config(&PORTA, 1 << PIN_PA07, &pin_conf );
    port_group_set_config(&PORTA, 1 << PIN_PA17, &pin_conf );

    PORTA.PINCFG[PIN_PA06].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA05].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA04].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA00].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA07].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA06].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA05].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA04].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA28].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA27].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA22].bit.DRVSTR = 0;
    PORTA.PINCFG[PIN_PA09].bit.DRVSTR = 0;

    delay_ms(1000);
    port_group_set_config(&PORTA, SEGMENT_PIN_PORT_MASK, &pin_conf );
    port_group_set_config(&PORTA, BANK_PIN_PORT_MASK, &pin_conf );

    delay_ms(1000);

    led_controller_init();
    led_controller_enable();


    /* Show a startup LED swirl */
    display_swirl(10, 200, 3, 60 );

    while (1) {

        led_set_intensity(led, ++intensity);
        delay_ms(500);
        led_off(led);
        led++;
        led= led % 60;


    }



}
