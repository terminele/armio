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

/* FIXME -- only tic_cb is used for right now */

typedef struct {

    /* Intialization routine for the control mode
     * called when transitioning into the mode
     * May be NULL */
    void (*enter_cb)(void);

    /* Main loop function for the control mode.  This
     * callback is called on every "tick" (i.e.
     * every ms) so the mode controller can perform
     * any updates or respond to events.  Returns true
     * when it is finished (i.e. main module should transition
     * to next control mode) */
    bool (*tic_cb)(event_flags_t event_flags, uint32_t tick_cnt);

    /* Number of ticks of inactivity that
     * can occur in this mode before we
     * should enter into sleep */
    uint32_t sleep_timeout_ticks;

    /* Called just before entering sleep so the
     * mode controller can perform any cleanup
     * functions. */
    void (*about_to_sleep_cb)(void);

    /* Called just after wakeup so the
     * mode controller can perform any wakeup
     * functions. */
    void (*wakeup_cb)(void);


} control_mode_t;

/* Main control mode index (i.e. time display control mode ) */
#define CONTROL_MODE_MAIN 0

//___ V A R I A B L E S ______________________________________________________

extern control_mode_t *control_mode_active;

//___ P R O T O T Y P E S ____________________________________________________

void control_mode_next( void );
  /* @brief move to next control mode
   * @param None
   * @retrn None
   */

void control_mode_select( uint8_t mode_index );
  /* @brief move to control mode with given index
   * @param index of control mode to select
   * @retrn None
   */

uint8_t control_mode_count( void );
  /* @brief total number of modes
   * @param None
   * @retrn mode count
   */

uint8_t control_mode_index( control_mode_t* mode_ptr );
  /* @brief index of given mode
   * @param None
   * @retrn index of given mode (0 to mode count - 1)
   */

void control_init( void );
  /* @brief initalize mode control module
   * @param None
   * @retrn None
   */


#endif /* end of include guard: __CONTROL_H__ */
