/** file:       display.h
  * author:     Richard Bryan
  *
  * display controller
  *
  * higher level control of leds
  *
  * e.g. led blinking, glowing, patterns
  *
  */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

//___ I N C L U D E S ________________________________________________________
#include <asf.h>

//___ M A C R O S ____________________________________________________________

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________

void display_init(void);
  /* @brief initialize display module
   * @param None
   * @retrn None
   */


void display_swirl(int tail_len, int tick_us, int revolutions);
  /* @brief display a 'snake' of leds swirling around the display
   * @param tail_len -- number of leds in the snake
   * @param tick_us -- microseconds between moving the head forward 1 led
   * @param swirl_count -- # of revolutions before returning
   * @retrn None
   */

#endif /* end of include guard */
// vim: shiftwidth=2
