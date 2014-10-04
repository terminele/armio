/** file:       leds.c
  * modified:   2014-10-03 17:11:10
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "leds.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________

short led_bank_pins[BANK_COUNT] = {
    PIN_PA17,
    PIN_PA18,
    PIN_PA25,
    PIN_PA24,
    PIN_PA23
};

short led_group_pins[GROUP_COUNT] = {
    PIN_PA16,
    PIN_PA15,
    PIN_PA14,
    PIN_PA11,
    PIN_PA07,
    PIN_PA06,
    PIN_PA05,
    PIN_PA04,
    PIN_PA28,
    PIN_PA27,
    PIN_PA22,
    PIN_PA19
};

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

//___ F U N C T I O N S ______________________________________________________

void led_enable( short led ) {
    port_pin_set_output_level(LED_BANK_PIN(led), true);
    port_pin_set_output_level(LED_GROUP_PIN(led), true);
}


void led_disable( short led ) {
    port_pin_set_output_level(LED_BANK_PIN(led), false);
    port_pin_set_output_level(LED_GROUP_PIN(led), false);
}


// vim: shiftwidth=2
