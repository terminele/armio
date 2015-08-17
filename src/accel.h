/** file:       accel.h
  * modified:   2014-11-03 11:49:42
  */

#ifndef __ACCEL_H__
#define __ACCEL_H__

//___ I N C L U D E S ________________________________________________________
#include "asf.h"
#include "main.h"

//___ M A C R O S ____________________________________________________________

#define ACCEL_VALUE_1G  32

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________
extern int16_t ax_fifo[32][3];
extern uint8_t ax_fifo_depth;
extern bool accel_wakeup_gesture_enabled;

//___ P R O T O T Y P E S ____________________________________________________

bool accel_data_read (int16_t *x_ptr, int16_t *y_ptr, int16_t *z_ptr);
 /* @brief reads the x,y,z data of the accelerometer
   * accelerometer
   * @param x,y,z - pointers to be filled with 10-bit signed acceleration data
   * @retrn true on success, false on failure
   */

uint8_t accel_x_click_count( void );
  /* @brief current number of active (pre-timeout) x clicks
   * @param None
   * @retrn number of clicks for pending click event
   */

event_flags_t accel_event_flags( void );
  /* @brief get any event flags from accelerometer
   * @param None
   * @retrn ev flags (e.g. SCLICK_X, DCLICK_Z, etc.)
   */

bool accel_wakeup_check( void );
  /* @brief confirm that the recent interrupt
   * should fully wake us up
   * @param None
   * @retrn true if system should fully wakeup
   */


void accel_enable ( void );
  /* @brief enable this module (i.e. wakeup accelerometer)
   * @param None
   * @retrn None
   */


void accel_sleep ( void );
  /* @brief disable this module (i.e. sleep accelerometer)
   * @param None
   * @retrn None
   */

void accel_init ( void );
  /* @brief initialize accelerometer interface
   * @param None
   * @retrn success or failure
   */

#endif /* end of include guard: __ACCEL_H__ */

// vim:shiftwidth=2
