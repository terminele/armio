/** file:       leds.h
  * modified:   2014-10-03 17:06:34
  * author:     Richard Bryan
  *
  * interface for controlling armio leds
  *
  * The 60 LEDs are multiplexed using 17
  * pins -- 5 pins to select the 'bank' that the
  * LED belongs to and 12 to select the 'group'.
  *
  * A single LED is enabled by pulling its bank pin
  * and group pin low.
  */

#ifndef __LEDS_H__
#define __LEDS_H__

//___ I N C L U D E S ________________________________________________________
#include <asf.h>

//___ M A C R O S ____________________________________________________________
#define BANK_COUNT 5
#define GROUP_COUNT 12


/* return the bank/group id for the given led */
#define LED_BANK(i) (i % 5)
#define LED_GROUP(i) (i/5)

/* return the bank/group pin for the given led */
#define LED_BANK_PIN(i) led_bank_pins[LED_BANK(i)]
#define LED_GROUP_PIN(i) led_group_pins[LED_BANK(i)]

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________
extern short led_bank_pins[BANK_COUNT];
extern short led_group_pins[GROUP_COUNT];

//___ P R O T O T Y P E S ____________________________________________________

void led_enable( short led );
  /* @brief enable the given led
   * @param led num (0-59)
   * @retrn None
   */

void led_disable( short led );
  /* @brief disable the given led
   * @param led num (0-59)
   * @retrn None
   */

#endif /* end of include guard: __LEDS_H__ */
// vim: shiftwidth=2
