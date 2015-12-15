/** file:       control.h
  * created:   2015-01-13 16:25:30
  * author:     Richard Bryan
  */

#ifndef __CONTROL_H__
#define __CONTROL_H__

//___ I N C L U D E S ________________________________________________________
#include <asf.h>
#include "main.h"

//___ M A C R O S ____________________________________________________________

//___ T Y P E D E F S ________________________________________________________

typedef bool (*tic_fun_t)(event_flags_t event_flags);

typedef struct {

    /* Main loop function for the control mode.  This
     * callback is called on every "tick" (i.e.
     * every ms) so the mode controller can perform
     * any updates or respond to events.  Returns true
     * when it is finished (i.e. main module should transition
     * to next control mode) */
    tic_fun_t tic_cb;

    /* Number of ticks of inactivity that
     * can occur in this mode before we
     * should enter into sleep */
    uint32_t sleep_timeout_ticks;

} ctrl_mode_t;


/* Main control mode index (i.e. time display control mode ) */
#define CONTROL_MODE_SHOW_TIME  0
#define CONTROL_MODE_SET_TIME   1
#define CONTROL_MODE_SELECTOR   2

#define IS_CONTROL_MODE_SHOW_TIME() \
     (control_mode_index(ctrl_mode_active) == CONTROL_MODE_SHOW_TIME)

//___ V A R I A B L E S ______________________________________________________

extern ctrl_mode_t *ctrl_mode_active;

//___ P R O T O T Y P E S ____________________________________________________

void control_mode_set( uint8_t mode_index);
  /* @brief set current control mode
   * @param index of control mode
   * @retrn None
   */

uint8_t control_mode_count( void );
  /* @brief total number of modes in current control state
   * @param None
   * @retrn mode count
   */

uint8_t control_mode_index( ctrl_mode_t* mode_ptr );
  /* @brief control mode
   * @param None
   * @retrn index of given mode (0 to mode count - 1)
   */


void control_tic( event_flags_t ev_flags);
  /* @brief tic current control mode 
   * @param event flags
   * @retrn None
   */

void control_init( void );
  /* @brief initalize mode control module
   * @param None
   * @retrn None
   */


#endif /* end of include guard: __CONTROL_H__ */

// vim:shiftwidth=2
