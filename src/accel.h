/** file:       accel.h
  * modified:   2014-11-03 11:49:42
  */

#ifndef __ACCEL_H__
#define __ACCEL_H__

//___ I N C L U D E S ________________________________________________________
#include <asf.h>

//___ M A C R O S ____________________________________________________________

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________

bool accel_data_read (int16_t *x_ptr, int16_t *y_ptr, int16_t *z_ptr);
 /* @brief reads the x,y,z data of the accelerometer
   * accelerometer
   * @param x,y,z - pointers to be filled with 10-bit signed acceleration data
   * @retrn true on success, false on failure
   */

void accel_enable ( void );
  /* @brief enable this module (i.e. wakeup accelerometer)
   * @param None
   * @retrn None
   */


void accel_disable ( void );
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
