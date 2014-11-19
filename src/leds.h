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
#define BRIGHT_RES          8       /* bit resolution */
#define BLINK_RES           0       /* bit resolution */

#define BRIGHT_DEFAULT      ((1 << BRIGHT_RES) - 1) // default brightness level
#define BLINK_DEFAULT       0       // default blink (0 == NONE)

#define BLINK_FACTOR        5       /* ratio of blink units to vision persist interval */
#define MAX_BRIGHT_VAL      ((1 << BRIGHT_RES) - 1)
#define MAX_BLINK_VAL       (BLINK_FACTOR * (1 << BLINK_RES) - 1)

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________
void led_controller_init( void );
  /* @brief initialize led module
   * @param None
   * @retrn None
   */

void led_controller_disable( void );
  /* @brief disable the led controller
   * @param None
   * @retrn None
   */

void led_controller_enable( void );
  /* @brief enable the led controller
   * @param None
   * @retrn None
   */

void led_on( uint8_t led );
  /* @brief enable the given led with default
   * brightness and no blink
   * @param led num (0-59)
   * @retrn None
   */

void led_off( uint8_t led );
  /* @brief disable the given led
   * @param led num (0-59)
   * @retrn None
   */

void led_clear_all( void );
  /* @brief disable all leds
   * @param none
   * @retrn None
   */

void led_set_intensity( uint8_t led, uint8_t intensity );
  /* @brief set led intensity
   * @param led num (0-59)
   * @param intensity (brightness) value
   * @retrn None
   */

void led_set_blink( uint8_t led, uint8_t blink);
  /* @brief set led blink level
   * @param led num (0-59)
   * @param intensity (brightness) value
   * @retrn None
   */

void led_set_state( uint8_t led_index, uint8_t blink, uint8_t intensity);
  /* @brief set intensitity and blink level
   * @param None
   * @retrn None
   */


#endif /* end of include guard: __LEDS_H__ */
// vim: shiftwidth=2
