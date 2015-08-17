/** file:       utils.h
  * created:    2015-02-25 11:16:26
  * author:     Richard Bryan
  */

#ifndef __UTILS_H__
#define __UTILS_H__

//___ I N C L U D E S ________________________________________________________

//___ M A C R O S ____________________________________________________________

//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________

void utils_spin_tracker_start( uint8_t initial_pos );
  /* @brief start "spin tracker" mode.  the
   * accelerometer will be used to keep track
   * of a virtual ball rolling on the edge
   * of the board.
   * @param initial_pos - location (0-59) to start the tracker "ball" at
   * @retrn None
   */

void utils_spin_tracker_end ( void );
  /* @brief end the spin tracking mode
   * @param None
   * @retrn None
   */

uint8_t utils_spin_tracker_update ( void );
  /* @brief update the spin tracker position
   * based on accelerometer and return a
   * number between 0 and 59 indicating current
   * location on board edge
   * @param None
   * @retrn 6-degree location 0-59
   */

/* functionality to read light sensor */
uint8_t adc_light_value_scale ( uint16_t value );
  /* @brief scales a light adc read quasi-logarithmically
   * for displaying on led ring
   * @param 16-bit adc value
   * @retrn led index to display
   */


uint8_t adc_vbatt_value_scale ( uint16_t value );
  /* @brief scales the raw vbat readings for displaying on led ring
   * @param 16-bit adc value
   * @retrn led index to display
   */


#endif /* end of include guard: __UTILS_H__ */

// vim:shiftwidth=2
