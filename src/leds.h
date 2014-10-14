/** file:       leds.h
  * modified:   2014-10-03 17:06:34
  * author:     Richard Bryan
  *
  * interface for controlling armio leds
  *
  * The 60 LEDs are multiplexed using 17
  * pins -- 5 pins to select the 'bank' that the
  * LED belongs to and 12 to select the 'segment'.
  *
  * A single LED is enabled by pulling its bank pin
  * and segment pin low.
  */

#ifndef __LEDS_H__
#define __LEDS_H__

//___ I N C L U D E S ________________________________________________________
#include <asf.h>

//___ M A C R O S ____________________________________________________________
#define BRIGHT_RES          3       /* bit resolution */
#define BLINK_RES           4       /* bit resolution */

#define BRIGHT_DEFAULT      ((1 << BRIGHT_RES) - 1) // default brightness level
#define BLINK_DEFAULT       0       // default blink (0 == NONE)

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________
void led_init( void );
  /* @brief initialize led module
   * @param None
   * @retrn None
   */

void led_enable( uint8_t led );
  /* @brief enable the given led
   * @param led num (0-59)
   * @retrn None
   */

void led_disable( uint8_t led );
  /* @brief disable the given led
   * @param led num (0-59)
   * @retrn None
   */

void led_set( uint8_t led, uint8_t intensity );
  /* @brief set led intensity with default blink (none)
   * @param led num (0-59)
   * @param intensity (brightness) value
   * @retrn None
   */

void led_set_state( uint8_t led_index, uint8_t blink, uint8_t light );
  /* @brief initialize led module
   * @param None
   * @retrn None
   */


#endif /* end of include guard: __LEDS_H__ */
// vim: shiftwidth=2
