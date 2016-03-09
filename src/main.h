/** file:       main.h
  * created:    2015-01-09 13:37:30
  * author:     Richard Bryan
  */

#ifndef __MAIN_H__
#define __MAIN_H__

//___ I N C L U D E S ________________________________________________________

//___ M A C R O S ____________________________________________________________


/* Event Flags */
#define EV_FLAG_NONE                    0
#define EV_FLAG_SINGLE_BTN_PRESS_END    (1 << 1)
#define EV_FLAG_LONG_BTN_PRESS          (1 << 2)
#define EV_FLAG_LONG_BTN_PRESS_END      (1 << 3)
#define EV_FLAG_ACCEL_CLICK             (1 << 4)
#define EV_FLAG_ACCEL_FAST_CLICK_END    (1 << 5)
#define EV_FLAG_ACCEL_SLOW_CLICK_END    (1 << 6)
#define EV_FLAG_ACCEL_DOWN_UP           (1 << 21)
#define EV_FLAG_ACCEL_DOWN             (1 << 22)
#define EV_FLAG_SLEEP                   (1 << 23)

/* Error Codes */
typedef enum {
  error_group_none      = 0,
  error_group_accel,
  error_group_disp,
  error_group_animation,
  error_group_control,
  error_group_assertion,
  error_group_rtc,
} error_group_code_t;


/* System (main) timer */
#define MS_PER_TICK             1
#define MS_IN_TICKS(M)         ((M)/MS_PER_TICK)
#define TICKS_IN_MS(T)         ((T)*MS_PER_TICK)
#define MAIN_TIMER_TICK_US      (MS_PER_TICK*1000)

#define LIGHT_SENSE_ENABLE_PIN       PIN_PA02

#define assert(B) if (!(B)) main_terminate_in_error( error_group_assertion, 0 )

/* Starting flash address at which to store data */
#define NVM_ADDR_START      ((1 << 15) + (1 << 14) + (1 << 13)) /* assumes program size < 56KB */
#define NVM_DATA_ADDR       NVM_ADDR_START
#define NVM_DATA_STORE_SIZE NVMCTRL_ROW_SIZE //256 bytes

//___ T Y P E D E F S ________________________________________________________
typedef uint32_t event_flags_t;

typedef enum sensor_type_t {
    sensor_vbatt,
    sensor_light,
} sensor_type_t;

typedef struct {
    /* configuration data and usage stats stored in flash
     * If updating this struct, ensure size does not
     * exceed NVM_DATA_STORE_SIZE
     */
    int8_t rtc_freq_corr;
    uint32_t lifetime_wakes;
    uint32_t filtered_gestures;
    uint32_t filtered_dclicks;
    uint32_t lifetime_ticks;
    uint32_t lifetime_resets;
    uint16_t wdt_resets;
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    bool     pm;

} nvm_data_t;

typedef struct {
    bool wake_gestures;
    bool seconds_always_on;
} user_data_t;

//___ V A R I A B L E S ______________________________________________________
extern nvm_data_t main_nvm_data;
extern user_data_t main_user_data;

//___ P R O T O T Y P E S ____________________________________________________
void main_terminate_in_error( error_group_code_t error_group, uint32_t subcode );
  /* @brief terminate program with error code
   * @param error group from enumeration list
   * @param subcode : only the first 3 bytes are currently used
   * @retrn None
   */

void main_start_sensor_read ( void );
  /* @brief start an adc read of current sensor
   * @param None
   * @retrn None
   */

sensor_type_t main_get_current_sensor ( void );
  /* @brief get current sensor setting
   * @param None
   * @retrn None
   */

void main_set_current_sensor ( sensor_type_t sensor);
  /* @brief set sensor to read adc values from
   * @param None
   * @retrn None
   */

uint16_t main_read_current_sensor( bool blocking );
  /* @brief read adc for most recently started sensor read
   * the current sensor is based on the most recent sensor_read() call
   * @param blocking - dont use cached value -- blocks until new value
   * @retrn adc value for the current sensor
   */

uint16_t main_get_light_sensor_value ( void );
  /* @brief get most recent light sensor adc value
   * @param None
   * @retrn None
   */

uint16_t main_get_vbatt_value ( void );
  /* @brief get running-averaged vbatt value
   * @param None
   * @retrn
   */

uint8_t main_get_batt_8bit( void );
  /* @brief return battery voltage as 8-bit unsigned int
   * @retrn battery voltage
   */

bool main_is_low_vbatt ( void );
  /* @brief check if battery is low
   * @param None
   * @retrn true if battery is low
   */

uint8_t main_get_multipress_count( void );
  /* @brief get number of taps for a multi-tap event
   * @param None
   * @retrn # of presses
   */

void main_log_data( uint8_t *data, uint16_t length, bool flush );
  /* @brief store data in unused flash space
   * @param data - pointer to data array
   * @param len - number of 32-bit words (byte count / 4) to write
   * @param flush - if true, write immediately
   * @retrn None
   */

uint32_t main_get_button_hold_ticks( void );
  /* @brief # of button ticks since long press started
   * @param None
   * @retrn # of button ticks since long press started
   */

uint32_t main_get_waketicks( void );
    /* @brief get then current value for waketicks
     * @param None
     * @retrn None
     */

uint32_t main_get_waketime_ms( void );
  /* @brief get time since last wake in ms
   * @param None
   * @retrn time awake in ms
   */

void main_inactivity_timeout_reset( void );
  /* @brief resets inativity timeout counter
   * @param None
   * @retrn None
   */

void main_deep_sleep_enable( void );
  /* @brief enable deep sleep/shipping mode
   * @param None
   * @retrn None
   */

void main_set_gesture_enable_preference( bool enable );
  /* @brief update user preference for enabling of wakeup gestures
   * @param enable boolean
   * @retrn None
   */
#endif /* end of include guard: __MAIN_H__ */

// vim:shiftwidth=2
