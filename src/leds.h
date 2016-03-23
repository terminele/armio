/** file:       leds.h
  * modified:   2014-10-03 17:06:34
  * author:     Richard Bryan
  *
  * interface for controlling leds
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
#define BRIGHT_LEVELS       5

#define BRIGHT_DEFAULT      4

#define MAX_BRIGHT_VAL      BRIGHT_LEVELS
#define MIN_BRIGHT_VAL      1

#define BRIGHT_LOW          MIN_BRIGHT_VAL
#define BRIGHT_MED_LOW      2
#define BRIGHT_MED          4
#define BRIGHT_HIGH         5
#define BRIGHT_MAX          MAX_BRIGHT_VAL

/* Non-pwm blink of LEDs -- bypassing the LED TC controller */
#define _BLINK( i )  do { \
      _led_on_full( i ); \
      delay_ms(100); \
      _led_off_full( i ); \
      delay_ms(50); \
      _led_on_full( i ); \
      delay_ms(100); \
      _led_off_full( i ); \
      delay_ms(50); \
    } while(0);
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

void led_controller_conf_output( void );
  /* @brief configure led pins for output
   * @brief enables _led_on/_led_off to function without
   * @brief running timer counters
   * @param None
   * @retrn None
   */

void led_controller_enable( void );
  /* @brief enable the led controller
   * @param None
   * @retrn None
   */

void led_set_intensity( uint8_t led, uint8_t intensity );
  /* @brief set led intensity
   * @param led num (0-59)
   * @param intensity (brightness) value
   * @retrn None
   */

static inline void led_on ( uint8_t led, uint8_t intensity ) {
    led_set_intensity( led, intensity );
}
  /* @brief enable the given led at the given brightness
   * @param led num (0-59)
   * @retrn None
   */

static inline void led_off ( uint8_t led ) { led_set_intensity(led, 0 );}
  /* @brief disable the given led
   * @param led num (0-59)
   * @retrn None
   */

void led_clear_all( void );
  /* @brief disable all leds
   * @param none
   * @retrn None
   */

void led_set_max_brightness( uint8_t brightness);
  /* @brief set global max brightness
   * @param brightness level
   * @retrn None
   */

void _led_on_full( uint8_t led );
  /* @brief bypass pwm and turn on led via gpio
   * @param led to turn on
   * @retrn None
   */

void _led_off_full( uint8_t led );
  /* @brief bypass pwm and turn off led via gpio
   * @param led to turn off
   * @retrn None
   */

#endif /* end of include guard: __LEDS_H__ */

// vim:shiftwidth=2
