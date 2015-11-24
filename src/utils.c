/** file:       utils.c
  * created:    2015-02-25 11:18:42
  * author:     Richard Bryan
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"
#include "main.h"
#include "utils.h"

#include <math.h>

//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define PI 3.14159265

#define RAD_2_DEG(rad) (180 * rad/PI )

/* Given an angle in degrees, round to nearest clock minute hand position */
#define CLOCK_POS(angle_deg) ((((int32_t)(90 + (360 - angle_deg)) % 360) + 3)/6)

/* Given a pos 0-59, give unit circle angle in degrees (e.g. 15 is 0 deg) */
#define POS_2_DEG(pos) (90 + 360 - (pos* 6.0))

/* Convert angle difference to shortest distance on circle */
#define ANGLE_DELTA_SHORTEST(a) ((a) > 180 ? (a - 360) : (a) < -180 ? (a + 360) :a)

/* Exponential average weight for updating angular velocity */
#define W_ALPHA 0.75

/* Minimum angular velocity value */
#define MIN_ANG_VEL 2.0

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________
static float tracker_pos_angle;
static int8_t tracker_pos;
static uint16_t z_avg = ACCEL_VALUE_1G;
static float angular_velocity = 0; /* in degrees/tick */

//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

//___ F U N C T I O N S ______________________________________________________

void utils_spin_tracker_start( uint8_t initial_pos ) {
    tracker_pos = initial_pos;
    tracker_pos_angle = POS_2_DEG(initial_pos);
}

void utils_spin_tracker_end ( void ) {
  //nothing to do currently
  //may use higher resolution/frequency accelerometer
  //mode in future that we'd revert back to default here
}

uint8_t utils_spin_tracker_update ( void ) {
    int16_t x, y, z;
    float angle, angle_delta;

    if (!accel_data_read(&x, &y, &z)) {
        return tracker_pos;
    }

    /* Exp average z to filter out high frequency vals
     * This allows us to still update position at
     * small tilts, but also to keep the position stable
     * when flat (i.e. z close to 1G) */
    z_avg/=8;
    z_avg+=abs(z)*7/8;

    if (abs(z) > ACCEL_VALUE_1G*7/8 ||
        abs(z_avg) > ACCEL_VALUE_1G*7/8) {
        return tracker_pos;
    }

    if (abs(x) + abs(y) < 5) return tracker_pos;

    angle = RAD_2_DEG(atan2(-y, -x));
    /* atan return [-180,180], normalize to [0, 360] */

    if (angle < 0) angle +=360;
    angle_delta = ANGLE_DELTA_SHORTEST(angle - tracker_pos_angle);

    /* Ignore small changes */
    if (abs(angle_delta) < 1) {
        angular_velocity = 0;
        return tracker_pos;
    }

    /* Update angular velocity */
    angular_velocity *= (1 - W_ALPHA);
    angular_velocity += W_ALPHA*angle_delta * (abs(y) + abs(x));

    if (abs(angular_velocity) < MIN_ANG_VEL)
        angular_velocity = angular_velocity < 0 ? -MIN_ANG_VEL : MIN_ANG_VEL;

    tracker_pos_angle += angular_velocity/1000.0;

    /* Ensure angle is in [0, 360] */
    while(tracker_pos_angle > 360) tracker_pos_angle-=360;
    while(tracker_pos_angle < 0) tracker_pos_angle+=360;

    tracker_pos = CLOCK_POS(tracker_pos_angle);
    /* Check for edge case rounding errors */
    if (tracker_pos > 59) tracker_pos = 59;
    if (tracker_pos < 0) tracker_pos = 0;

    return (uint8_t)tracker_pos;
}

uint8_t adc_light_value_scale ( uint16_t value ) {
    /* Assumes 16-bit averaged adc read */

    /* Decimate down to 12-bit */
    value >>= 4;

    if (value >= 2048)
        return 50 + ((value - 2048 ) >> 8);

    if (value >= 1024)
        return 42 + ((value - 1024) >> 7);

    if (value >= 512)
        return 34 + ((value - 512) >> 6);

    if (value >= 256)
        return 26 + ((value - 256) >> 5);

    if (value >= 128)
        return 18 + ((value - 128) >> 4);

    if (value >= 64)
        return 11 + ((value - 64) >> 3);

    if (value >= 32)
        return 7 + ((value - 32) >> 3);

    return value >> 2;
}

uint8_t adc_vbatt_value_scale ( uint16_t value ) {
    /* Assumes 16-bit averaged adc read */
    /* Decimate down to 12-bit */
    value >>= 4;

    /* Full */
    if (value >= 3000) //> 3V --> 3/4*4096
        return 0;

    /* Greater than 3/4 full */
    if (value >= 2900) // ~2.9V 
        return 45 + 14*(value - 2900)/100;

    /* Greater than 1/2 full */
    if (value >= 2800) // ~2.8V --> 2.8/4*4096
        return 30 + 14*(value - 2800)/100;

    /* between 1/4 and 1/2 full */
    if (value >= 2700) // ~2.7V
        return 15 + 14*(value - 2700)/100;

    /* between 1/8 and 1/4 full */
    if (value >= 2600) // ~2.5
        return 7 + 7*(value - 2600)/100;

    if (value <= 2048) // < 2V
        return 1;

    return 1 + 7*(value - 2048)/(2600 - 2048);
}

// vim:shiftwidth=2
