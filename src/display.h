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
#include "leds.h"

//___ M A C R O S ____________________________________________________________

//___ T Y P E D E F S ________________________________________________________

typedef enum {
  dispt_unused = 0,
  dispt_point,
  dispt_line,
  dispt_snake,
  dispt_polygon
} display_type_t;

typedef struct display_comp_t {
  bool on;
  display_type_t type;

  uint8_t brightness;
  int8_t pos;
  int8_t length;

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

display_comp_t* display_point ( int8_t pos,
        uint8_t brightness );
  /* @brief display a single point (led) at the given pos
   * @param pos -- led number 0-59
   * @param brightness - led intensity/brightness
   * @retrn handle to a display structure representing the
   *   the point being displayed
   */

display_comp_t* display_line ( int8_t pos,
        uint8_t brightness, int8_t length);
  /* @brief display a line of points (leds) starting at the given pos
   *     and ending clockwise n points from there
   * @param pos - head led number 0-59 (highest point of line clockwise)
   * @param brightness - led intensity/brightness
   * @param length - length of line
   * @retrn handle to a display structure representing the
   *   the line being displayed
   */


display_comp_t* display_snake ( int8_t pos,
        uint8_t brightness, int8_t length);
  /* @brief same as display_line() but led intensity
   *    decreases with distance from head led
   * @param pos - head led number 0-59 (highest point of line clockwise)
   * @param brightness - led intensity/brightness
   * @param length - length of snake
   * @retrn handle to a display structure representing the
   *   the snake
   */

display_comp_t* display_polygon ( int8_t pos,
        uint8_t brightness, int8_t num_sides);
  /* @brief display a polygon of points (leds) starting at the given pos
   * @param pos - starting vertex (led number 0-59)
   * @param brightness - led intensity/brightness
   * @param num sides - # of sides/vertices of polygon
   * @retrn handle to a display structure representing the
   *   the polygon being displayed
   */


void display_comp_update_pos ( display_comp_t *ptr, int8_t pos );
  /* @brief update the pos position of the given component
   * @param comp_ptr - handle to component to update
   * @param new_pos - new position for component
   * @retrn None
   */

void display_comp_hide (display_comp_t *ptr);
  /* @brief hide the given component from displaying
   * @param component handle to hide
   * @retrn None
   */

void display_comp_hide_all ( void );
  /* @brief hide all current display components
   * @retrn None
   */

void display_comp_show_all ( void );
  /* @brief show all current display components
   * @retrn None
   */

static inline void display_comp_show (display_comp_t *ptr) { ptr->on = true; }
  /* @brief show a previously hidden display component
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


#endif /* end of include guard */
