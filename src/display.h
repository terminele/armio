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

#define BLINK_NONE  0

//___ T Y P E D E F S ________________________________________________________

typedef enum {
  dispt_unused = 0,
  dispt_point,
  dispt_line,
  dispt_snake,
  dispt_polygon
} display_type_t;

typedef struct display_comp_t {
  display_type_t type;
  bool on;

  uint16_t blink_interval;
  uint8_t brightness;
  uint8_t pos;
  uint8_t length;

  struct display_comp_t *next, *prev;


} display_comp_t;

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________

void display_init(void);
  /* @brief initialize display module
   * @param None
   * @retrn None
   */

void display_tic(void);
  /* @brief update display
   * @param None
   * @retrn None
   */

display_comp_t* display_point ( uint8_t pos,
        uint8_t brightness, uint16_t blink_interval );
  /* @brief display a single point (led) at the given pos
   * @param pos -- led number 0-59
   * @param brightness - led intensity/brightness
   * @param blink_interval - blink interval in milliseconds
   * @retrn handle to a display structure representing the
   *   the point being displayed
   */

display_comp_t* display_line ( uint8_t pos,
        uint8_t brightness, uint16_t blink_interval,
        uint8_t length);
  /* @brief display a line of points (leds) starting at the given pos
   *     and ending clockwise n points from there
   * @param pos - head led number 0-59 (highest point of line clockwise)
   * @param brightness - led intensity/brightness
   * @param blink_interval - blink interval in milliseconds
   * @param length - length of line
   * @retrn handle to a display structure representing the
   *   the point being displayed
   */

static inline void display_comp_update_pos ( display_comp_t *ptr, uint8_t pos ) {
    ptr->pos = pos;
}
  /* @brief update the pos position of the given component
   * @param comp_ptr - handle to component to update
   * @param new_pos - new position for component
   * @retrn None
   */

static inline void display_comp_hide (display_comp_t *ptr) { ptr->on = false; }
  /* @brief hide the given component from displaying
   * @param component handle to hide
   * @retrn None
   */

static inline void display_comp_show (display_comp_t *ptr) { ptr->on = true; }
  /* @brief show the previously hidden display component
   * @param component handle to show
   * @retrn None
   */

void display_comp_release (display_comp_t *comp_ptr);
  /* @brief clear the given component from the display.  the
   *   component handle will no longer be valid for caller
   * @param component handle to destroy
   * @retrn None
   */

void display_refresh(void);
  /* @brief refresh display
   * @param None
   * @retrn None
   */

void display_swirl(int tail_len, int tick_us,
    int revolutions, int max_intensity);
  /* @brief display a 'snake' of leds swirling around the display
   * @param tail_len -- number of leds in the snake
   * @param tick_us -- microseconds between moving the head forward 1 led
   * @param swirl_count -- # of revolutions before returning
   * @param max_intensity -- brightness of the head led
   * @retrn None
   *
   * ### doesnt work very well with tail longer than ~20
   */



#endif /* end of include guard */
// vim: shiftwidth=2
