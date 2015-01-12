/** file:       main.h
  * created:    2015-01-09 13:37:30
  * author:     Richard Bryan
  */

#ifndef __CONTROL_H__
#define __CONTROL_H__

//___ I N C L U D E S ________________________________________________________

//___ M A C R O S ____________________________________________________________


/* Event Flags */
#define EV_FLAG_NONE                    0
#define EV_FLAG_SINGLE_BTN_PRESS_END    1 << 1
#define EV_FLAG_LONG_BTN_PRESS          1 << 2
#define EV_FLAG_LONG_BTN_PRESS_END      1 << 3

/* Error Codes */
#define ERROR_NONE                      0
#define ERROR_ACCEL_READ_ID             1
#define ERROR_ACCEL_BAD_ID              2
#define ERROR_ACCEL_WRITE_ENABLE        3
#define ERROR_DISP_DRAW_BAD_COMP_TYPE   4
#define ERROR_ANIM_BAD_TYPE             5
#define ERROR_ASSERTION_FAIL            6

#define assert(B) if (!(B)) main_terminate_in_error(ERROR_ASSERTION_FAIL)

//___ T Y P E D E F S ________________________________________________________
typedef uint16_t event_flags_t;

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________
void main_terminate_in_error( uint8_t error_code);
  /* @brief terminate program with error code
   * @param error flags
   * @retrn None
   */

uint8_t main_get_multipress_count( void );
  /* @brief get number of taps for a multi-tap event
   * @param None
   * @retrn # of presses
   */

uint32_t main_get_button_hold_ticks( void );
  /* @brief # of button ticks since long press started
   * @param None
   * @retrn # of button ticks since long press started
   */

#endif /* end of include guard: __CONTROL_H__ */
