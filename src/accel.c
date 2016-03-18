/** file:       accel.c
  * modified:   2014-11-03 11:50:33
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"
#include "lis2dh12.h"
#include "main.h"
#include "leds.h"
#include "aclock.h"

// TODO : on super Y, turn off when y low / z high


//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define ACCEL_ERROR_TIMEOUT             ((uint32_t) 1)
#define ACCEL_ERROR_SELF_TEST(test)    (((uint32_t) 2) \
    | (((uint32_t) test)<<8))
#define ACCEL_ERROR_READ_ID             ((uint32_t) 3)
#define ACCEL_ERROR_WRONG_ID(id)       (((uint32_t) 4) \
    | (((uint32_t) id)<<8) )
#define ACCEL_ERROR_CONFIG(reg, val)        \
    main_terminate_in_error( error_group_accel, \
            ((uint32_t) 5) | (((uint32_t) reg)<<8) | (((uint32_t) val)<<16) )

#ifndef ABS
#define ABS(a)       ( a < 0 ? -1 * a : a )
#endif

#ifndef LENGTH
#define LENGTH(a) (sizeof(a)/sizeof(a[0]))
#endif

#ifndef USE_SELF_TEST
# define USE_SELF_TEST false
#endif

/* for easier debugging of interrupt levels */
/* flash led to indicate isr triggering */
#ifndef DEBUG_AX_ISR
# define DEBUG_AX_ISR false
#endif
#ifndef REJECT_ALL_GESTURES
#define REJECT_ALL_GESTURES false
#endif
#ifndef SKIP_WAIT_FOR_DOWN
#define SKIP_WAIT_FOR_DOWN false
#endif


#ifndef GESTURE_FILTERS
# define GESTURE_FILTERS    true
#endif
/* filter gestures based on intentional 'views' */


#ifndef SHOW_ACCEL_ERRORS_ON_LED
#define SHOW_ACCEL_ERRORS_ON_LED false
#endif
/* show / disable accel related errors showing on LED's */

//#define SHOW_LED_FOR_FILTER_INFO true
#ifndef SHOW_LED_FOR_FILTER_INFO
#define SHOW_LED_FOR_FILTER_INFO false
#endif
/* show a debug led indicating the gesture filter result */

#ifndef SHOW_LED_ON_DCLICK_FAIL
#define SHOW_LED_ON_DCLICK_FAIL false
#endif
/* Do we flash an LED if double-click was triggerd but filtered out
 * because of the watch orientation */

#ifndef USE_PCA_LDA_FILTERS
#define USE_PCA_LDA_FILTERS false
#endif
/* Do we use new filters created from PCA / LDA analysis */

#ifndef WAKE_ON_SUPER_Y
#define WAKE_ON_SUPER_Y true
#endif
/* Do we allow a 'down' event to trigger with y or z high? */

#ifndef LOG_ACCEL_GESTURE_FIFO
#define LOG_ACCEL_GESTURE_FIFO false
#endif

#ifndef LOG_UNCONFIRMED_GESTURES
#define LOG_UNCONFIRMED_GESTURES true
#endif


#if ( SHOW_LED_FOR_FILTER_INFO )
#define _DISP_FILTER_INFO( i )  do { \
      _led_on_full( i ); \
      delay_ms(20); \
      _led_off_full( i ); \
      delay_ms(5); \
      _led_on_full( i ); \
      delay_ms(20); \
      _led_off_full( i ); \
      delay_ms(5); \
    } while(0);
#else   /* SHOW_LED_FOR_FILTER_INFO */
#define _DISP_FILTER_INFO( i )
#endif  /* SHOW_LED_FOR_FILTER_INFO */

#if ( SHOW_ACCEL_ERRORS_ON_LED )
#define _DISP_ERROR( i )  do { \
      _led_on_full( i ); \
      delay_ms(1000); \
      _led_off_full( i ); \
      delay_ms(100); \
      _led_on_full( i ); \
      delay_ms(1000); \
      _led_off_full( i ); \
      delay_ms(100); \
    } while(0);
#else   /* SHOW_ACCEL_ERRORS_ON_LED */
#define _DISP_ERROR( i )
#endif

#define DISP_ERR_FIFO_READ()            _DISP_ERROR( 30 )
#define DISP_ERR_CONSEC_READ_1()        _DISP_ERROR( 57 )
#define DISP_ERR_CONSEC_READ_2()        _DISP_ERROR( 58 )
#define DISP_ERR_CONSEC_READ_3()        _DISP_ERROR( 59 )
#define DISP_ERR_WAKE_1()               _DISP_ERROR( 19 )
#define DISP_ERR_WAKE_2()               _DISP_ERROR( 56 )
//#define DISP_ERR_WAKE_3()               _DISP_ERROR( 54 )
//#define DISP_ERR_WAKE_4()               _DISP_ERROR( 53 )
//#define DISP_ERR_WAKE_GEST( state )     _DISP_ERROR( state + 40 )
#define DISP_ERR_ISR_RELEASE()          _DISP_ERROR( 10 )

#define MS_TO_ODRS(t, sample_int) (t/sample_int)

#define BITS_PER_ACCEL_VAL 8
#define FIFO_MAX_SIZE   32

/* Click configuration constants */
#define ACTIVE_ODR              ODR_400HZ
#define ACTIVE_SAMPLE_INT       SAMPLE_INT_400HZ

#define SLEEP_ODR               ODR_100HZ
#define SLEEP_SAMPLE_INT        SAMPLE_INT_100HZ

/* parameters for ST sleep-to-wake/return-to-sleep functionality */
#define DEEP_SLEEP_THS      4
#define DEEP_SLEEP_DUR      MS_TO_ODRS(1000, SLEEP_SAMPLE_INT)

#define ACTIVE_CLICK_THS        55      /* assumes 4g scale */
#define ACTIVE_CLICK_TIME_LIM   MS_TO_ODRS(20, ACTIVE_SAMPLE_INT)
#define ACTIVE_CLICK_TIME_LAT   MS_TO_ODRS(50, ACTIVE_SAMPLE_INT) /* ms */

#define SLEEP_CLICK_THS         47      /* assumes 4g scale */
#define SLEEP_CLICK_TIME_LIM    MS_TO_ODRS(30,  SLEEP_SAMPLE_INT)
#define SLEEP_CLICK_TIME_LAT    MS_TO_ODRS(100, SLEEP_SAMPLE_INT) /* ms */

#define FAST_CLICK_WINDOW_MS 400
#define SLOW_CLICK_WINDOW_MS 1800
    /* manual multi-click settings for ACTIVE mode */

#define DCLICK_TIME_WIN         MS_TO_ODRS(400, SLEEP_SAMPLE_INT)
    /* automated double click settings for SLEEP mode
     * threshold : click checking starts when threshold is exceeded
     * limit : value must be back below threshold by 'limit'
     * latency : ignore from start of click through latency for a second click
     * window : starts at end of latency, 2nd click must start before end of win
     * details in AN3308 */


//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________
typedef enum { reject=-1, punt=0, accept=1 } fltr_result_t;

typedef enum {
    WAIT_FOR_DOWN = 0,
    WAIT_FOR_UP,
} wake_gesture_state_t;

typedef union {
    struct {
        bool xl     : 1;
        bool xh     : 1;
        bool yl     : 1;
        bool yh     : 1;
        bool zl     : 1;
        bool zh     : 1;
        bool ia     : 1;
        bool super  : 1;
    };
    uint8_t b8;
} int_reg_flags_t;

typedef union {
    struct {
        bool x      : 1;
        bool y      : 1;
        bool z      : 1;
        bool sign   : 1;
        bool sclick : 1;
        bool dclick : 1;
        bool ia     : 1;
        bool        : 1;
    };
    uint8_t b8;
} click_flags_t;

typedef struct {
    union {
        struct {
            short int               : 16 - BITS_PER_ACCEL_VAL;
            short int x_l   : BITS_PER_ACCEL_VAL;
        };
        short int x                 : 16;
    };
    union {
        struct {
            short int               : 16 - BITS_PER_ACCEL_VAL;
            short int y_l   : BITS_PER_ACCEL_VAL;
        };
        short int y                 : 16;
    };
    union {
        struct {
            short int               : 16 - BITS_PER_ACCEL_VAL;
            short int z_l   : BITS_PER_ACCEL_VAL;
        };
        short int z                 : 16;
    };
} accel_xyz_t;

typedef struct {
    union {
        accel_xyz_t     values[ FIFO_MAX_SIZE ];
        uint8_t         bytes[ FIFO_MAX_SIZE * sizeof( accel_xyz_t ) ];
    };
    uint8_t depth;
} accel_fifo_t;

/* Multiple Accumulate Compare Filter Def */
typedef struct {
    /* Filter action (accept, punt, reject) if lower than this threshold */
    const int32_t lower_ths; 
    const fltr_result_t lt_action;
    
    /* Filter action (accept, punt, reject) if greater than this threshold */
    const int32_t upper_ths;
    const fltr_result_t gt_action;
    
    /* weights for x,y,z fifo values for multiply accumulate */
    const int32_t x_ws[32];
    const int32_t y_ws[32];
    const int32_t z_ws[32];
} macc_fltr_t;

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
static void wait_state_conf( wake_gesture_state_t wait_state );
    /* @brief configure the interrupts to detect movement into orientations of
     *        interest, used for gesture wake
     * @param the state that we want to wait for
     * @retrn None
     */

static bool wake_check( void );

static inline bool gesture_filter_check( void );
    /* @brief read the accel fifo and filter turn to wake gestures
     * @param None
     * @retrn true if the watch should wake up
     */

static inline bool dclick_filter_check( void );
    /* @brief read the accel current position and filter double click
     * @param None
     * @retrn true if the watch should wake up
     */

#if ( LOG_ACCEL_GESTURE_FIFO )
static inline void log_accel_gesture_fifo( void );
    /* @brief log the fifo for an accel gesture
     * @param None
     * @retrn None
     */
#endif  /* LOG_ACCEL_GESTURE_FIFO */

static void accel_isr(void);

static void configure_i2c(void);
    /* @brief setup the i2c module to communicate with accelerometer
     * accelerometer
     * @param
     * @retrn none
     */

static inline bool fltr_y_not_deliberate_fail( int16_t y_last, accel_xyz_t* sums );
    /* @brief Highest priority filter is to check if the latest y
     *        is facing outward and the y-movement was not deliberate enough
     *        when turning wrist up from horizontal position, the sum of the
     *        first N y-values should exceed a certain threshold in the
     *        negative direction (i.e. 12 oclock facing downward)
     * @param y_last : the last (most recent) y value
     * @param sums : the sum history pointer
     * @retrn true on fail
     */

static inline bool fltr_z_sum_slope_accept( accel_xyz_t* sums );
    /* @brief the difference between the sum of the first N z-values and
     *        the sum of the last N z-values should exced a threshold
     *        (i.e. face moves from vertical to  horizontal)
     * @param the sum history pointer
     * @retrn true on pass
     */

static inline bool fltr_y_turn_arm_accept( accel_xyz_t* sums );
static inline bool fltr_x_turn_arm_accept( accel_xyz_t* sums );
static inline bool fltr_xy_turn_arm_accept( accel_xyz_t* sums );
    /* @brief if the sum after a certain number of values is high enough
     * @param the sum history pointer
     * @retrn true on pass
     */

static inline bool fltr_y_overshoot_accept( accel_xyz_t* sums );
    /* @brief edge case test for when watch gets into a position closer to
     *        looking at you, then falls back into more of a flat position
     * @param the sum history pointer
     * @retrn true on pass
     */

static inline bool fltr_y_facing_inwards_accept( int16_t y_last );
    /* @brief for when the watch is really facing inwards, this is a farily
     *        'uncomfortable' position that indicates you are looking at the
     *        watch
     * @param the last y value in the fifo
     * @retrn true on pass
     */

static bool accel_register_consecutive_read (uint8_t start_reg,
        uint8_t count, uint8_t *data_ptr);
    /* @brief reads a series of data from consecutive registers from the
     * accelerometer
     * @param start_reg - address of first register to read
     *        count -  # of consecutive registers to read
     *        data  -  pointer to array of data to be filled with reg values
     * @retrn true on success, false on failure
     */

static bool accel_register_write (uint8_t reg, uint8_t val);

#if ( USE_SELF_TEST )
static void run_self_test( void );
    /* @brief run the self test on the acceleration module
     *        expect an output change of between 17 and 360, defined by
     *        OUTPUT[LSb](self-test enabled) - OUTPUT[LSb](self-test disabled)
     *        where 1LSB=4mg at 10bit representation and +/- 2g full scale
     *        sign is defined in CTRL_REG4 (0x23)
     * @param None
     * @retrn None
     */
#endif

#if ( GESTURE_FILTERS || LOG_ACCEL_GESTURE_FIFO )
static void read_accel_fifo( void );
#endif

static bool check_tilt_down( int16_t x, int16_t y, int16_t z );
    /* @brief check if the watch it tilted away from the user and should be sent
     *        to sleep. this can be used while awake and before waking
     * @param current accel values
     * @retrn true when tilted down
     */

static bool check_tilt_not_viewable( int16_t x, int16_t y, int16_t z );
    /* @brief check if the watch is oriented in a postion that is not viewable
     * @param current accel values
     * @retrn true when tilted down
     */



//___ V A R I A B L E S ______________________________________________________
uint8_t accel_slow_click_cnt = 0;
uint8_t accel_fast_click_cnt = 0;

static accel_fifo_t accel_fifo = { .bytes = { 0 }, .depth = 0 };

static bool accel_wakeup_gesture_enabled = true;
#if (LOG_ACCEL_GESTURE_FIFO)
bool accel_confirmed = false;
#endif

static uint8_t slow_click_counter = 0;
static uint8_t fast_click_counter = 0;

static uint16_t i2c_addr = AX_ADDRESS0;

static uint32_t last_click_time_ms;

static struct i2c_master_module i2c_master_instance;

static click_flags_t click_flags;
static int_reg_flags_t int1_flags = { .super = false };
static int_reg_flags_t int2_flags = { .super = false };

static wake_gesture_state_t wake_gesture_state;

/* Gesture filter constants -- AUTO GENERATED by accel analysis script */
const macc_fltr_t zh_fltrs[] = 
{
 {
	/*** Z-FILTER 1 - Fix Weight no PCA (60%) ***/
	.lower_ths = 0,
	.lt_action = punt,
	.upper_ths = -446706,
	.gt_action = reject,
	.x_ws = {
     54012,      0,      0,   4167,  -2165,      0,      0,  -8856,
         0,      0,      0,      0,   1958,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,   1041,      0,   1789,      0,      0,      0,      0,
    },
	.y_ws = {
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,  -4167,    586,
         0,   2083,      0,      0,    533,      0, -65536,      0,
    },
	.z_ws = {
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,    520,      0,
         0,  31893,      0,      0,      0,      0,      0,      0,
         0,      0,      0, -48610,      0,  54752, -60013,  -2083,
    },
  },
 {
	/*** Z-FILTER 2 - Round 2 Fix Weight no PCA (54%) ***/
	.lower_ths = 0,
	.lt_action = punt,
	.upper_ths = 5230486,
	.gt_action = reject,
	.x_ws = {
         0,      0,      0,      0,   -512,      0,      0,      0,
         0,    256,      0,      0,      0,   2048,      0,      0,
         0,      0,      0, -42998,      0, -65536,      0,  18353,
         0,      0,  29418,      0, -16658,      0, -28211,      0,
    },
	.y_ws = {
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0, -58982,      0,      0,      0,      0,
    },
	.z_ws = {
         0,      0,      0,      0,  38698,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,  -4503,
         0, -13493,      0,  18038,      0,      0,   2882,   3602,
         0,      0,      0,      0,      0,  34828,  62634,  -4096,
    },
  },
 {
	/*** Z-FILTER 3 - Round 3 Y-turn accepts ***/
	.lower_ths = 0,
	.lt_action = punt,
	.upper_ths = -1749158,
	.gt_action = accept,
	.x_ws = {
     26156,  23497,  19475,  15669,   9098,   5498,     17,  -4861,
     -7906,  -9712,  -9168,  -7490,  -8205,  -8986, -10649,  -9484,
     -8574,  -8183,  -6794,  -6422,  -5725,  -4599,  -3422,   -604,
      -337,   -284,    167,    328,    212,    769,    783,   1146,
    },
	.y_ws = {
    -62271, -61932, -54030, -51469, -48088, -48552, -42286, -32404,
    -29369, -18587,  -9111,   3161,  12415,  17938,  30386,  36638,
     41782,  47074,  47740,  49115,  49536,  50535,  52156,  52696,
     52346,  51421,  50717,  48475,  46952,  45137,  44108,  45227,
    },
	.z_ws = {
    -65536, -62615, -58106, -48842, -38389, -28383, -17732,  -4534,
      7378,  20030,  23182,  25973,  22934,  21008,  16182,  12977,
      8315,   4880,   2105,    721,   -628,  -1429,  -2729,  -4981,
     -6127,  -7392,  -7990,  -8102,  -7017,  -5446,  -3947,  -3245,
    },
  },
 {
	/*** Z-FILTER 4 - Round 4 X-turn accepts ***/
	.lower_ths = 0,
	.lt_action = punt,
	.upper_ths = -17284718,
	.gt_action = accept,
	.x_ws = {
     23200,  16683,   9066,   9580,   4141,  -3962, -12810, -31655,
    -36521, -38897, -35085, -35851, -36346, -39118, -37378, -39760,
    -34636, -36256, -31652, -27804, -25864, -19212, -12286,  -7157,
     -4164,  -1562,   2518,   3528,   4049,   4963,   4682,   6877,
    },
	.y_ws = {
    -29583, -21903,   4622, -17452, -36467, -41474, -26352, -10512,
    -12657, -13850,  11788,  30983,  57374,  61410,  57328,  49723,
     54519,  65536,  60635,  60511,  62393,  62598,  61109,  60332,
     60468,  60394,  60987,  58525,  56137,  55249,  54650,  54682,
    },
	.z_ws = {
    -49395, -55834, -43578, -26345, -12388, -14322, -41967, -44087,
    -36964, -25421, -53619, -60921, -63159, -45876, -43926, -22894,
    -18644,  -9957, -12400, -13712, -14677, -16324, -17469, -19462,
    -19038, -16246, -14383, -13155, -10381,  -7032,  -4119,  -3944,
    },
  },
 {
	/*** Z-FILTER 5 - Round 5, final ***/
	.lower_ths = 0,
	.lt_action = accept,
	.upper_ths = 0,
	.gt_action = reject,
	.x_ws = {
     -3955,   4483,   8721,   4478,  -2022,  -2327,   5260,   9801,
     12839,  13467,  16849,  19960,  21789,  24624,  27936,  31324,
     32231,  35895,  36346,  35233,  35162,  35665,  35846,  35786,
     35141,  34534,  33063,  32611,  31205,  30463,  29131,  28826,
    },
	.y_ws = {
    -12697,  -1905,  -2646, -13055, -23760, -36802, -46602, -54816,
    -54527, -53567, -60140, -61642, -65536, -61325, -62674, -60174,
    -60386, -56689, -46092, -39149, -35981, -34129, -31283, -29238,
    -28368, -28082, -28091, -28353, -26563, -24307, -23030, -22227,
    },
	.z_ws = {
     -8696, -19425, -22185, -17831, -11460,    792,   9893,  14622,
      9644,  12360,  13187,  15867,  10914,   6411,   8049,   6711,
      7309,   7154,   6138,   6932,   6753,   6420,   5155,   4056,
      3422,   2294,   2685,   3483,   3449,   3483,   2636,   1561,
    },
  },
};

const macc_fltr_t yh_fltrs[] = 
{
 {
	/*** Y-FILTER 1 - PCA8, Reject Test 1 (45%) ***/
	.lower_ths = 2733591,
	.lt_action = reject,
	.upper_ths = 34764674,
	.gt_action = reject,
	.x_ws = {
    -28247, -26720, -25102, -23099, -20643, -19454, -16719, -16150,
    -16157, -16853, -16253, -15324, -14883, -13047, -12133, -10941,
    -10783, -10014,  -9170,  -8288,  -7870,  -7449,  -6238,  -5228,
     -4938,  -4839,  -5351,  -4902,  -4428,  -3490,  -3509,  -3143,
    },
	.y_ws = {
     18514,  18540,  18072,  15983,  12543,   6023,  -1134,  -9280,
    -14424, -15368, -17980, -18404, -20052, -15961,  -8334,    451,
      6143,   9584,  10472,   9282,   8543,   5502,   2296,   1415,
      -451,  -2813,  -5286,  -7808, -10275, -13854, -16315, -16679,
    },
	.z_ws = {
       572,    425,   1331,   -877,  -1846,  -6950,  -9807, -13554,
    -17902, -19724, -20019, -20088, -19714, -17269, -14184, -10445,
     -3591,   4126,  13228,  23469,  31653,  38310,  45687,  49847,
     54996,  59684,  64015,  65536,  65057,  63945,  62580,  59360,
    },
  },
 {
	/*** Y-FILTER 2 - PCA8, Reject Test 2 (22%) ***/
	.lower_ths = -89832048,
	.lt_action = reject,
	.upper_ths = 29154581,
	.gt_action = reject,
	.x_ws = {
     57961,  61043,  63047,  63874,  64606,  65536,  65195,  64894,
     61653,  59486,  57407,  58174,  55736,  53252,  49559,  45092,
     42341,  37142,  33296,  30578,  27535,  24511,  17972,  15209,
     13356,  10583,   8208,   5450,   3449,   3012,   2127,   2375,
    },
	.y_ws = {
     42129,  45340,  44132,  43850,  48841,  47395,  50848,  51593,
     53497,  52777,  47470,  41659,  30621,  17853,   2120, -14609,
    -25616, -29882, -27270, -25170, -25434, -24403, -25020, -29960,
    -33557, -36306, -35631, -32567, -30214, -25485, -24595, -23653,
    },
	.z_ws = {
     59988,  61252,  61520,  60255,  58120,  54404,  48918,  42721,
     39579,  35189,  29771,  25172,  21581,  16288,   9839,   4090,
      1298,   -272,    641,   1223,   2904,   5322,   6212,   8401,
      8260,   9118,   9555,  10099,  11304,  11484,  10890,  11501,
    },
  },
 {
	/*** Y-FILTER 3 - Y-trig X-turn Accept (PCA8, FP 16%) ***/
	.lower_ths = 3269748,
	.lt_action = accept,
	.upper_ths = 0,
	.gt_action = punt,
	.x_ws = {
     65536,  53201,  43038,  34966,  25535,  17619,   9357,   1228,
     -4127,  -8943, -11623, -15508, -18465, -20734, -21103, -19894,
    -20036, -15790, -11932, -11583,  -8788,  -8774,  -2441,  -1155,
       -12,   1443,   3094,   4335,   4473,   2330,   2197,   2702,
    },
	.y_ws = {
     15181,   8550,   3726,  -2067,  -6330,  -9336, -11567, -12808,
    -16538, -18098, -16700, -19178, -19090, -15092, -10560,    368,
      7159,  11027,  11345,  13255,  20641,  29159,  38235,  48710,
     54367,  59128,  61376,  58651,  57409,  52008,  47989,  41592,
    },
	.z_ws = {
     15238,  13208,  11740,   8631,   7531,  10410,   9895,   8488,
      5775,   2729,    -18,   -917,  -3356,  -5132,  -7232,  -9073,
     -6991,  -6219,  -3191,  -4992,  -8170,  -9965,  -9885, -12373,
    -12158, -11830, -11633, -12837, -14729, -16347, -16449, -20403,
    },
  },
 {
	/*** Y-FILTER 4 - Y-trig Y-turn Accept (PCA8, FP 14%) ***/
	.lower_ths = -13211549,
	.lt_action = accept,
	.upper_ths = 0,
	.gt_action = punt,
	.x_ws = {
     -6274,  -6995,  -6853,  -5462,  -3768,  -3804,  -1949,   -627,
      2644,   4716,   6570,   6000,   5023,   4181,   4146,   7215,
      7001,   6894,   5917,   3448,   3488,   3478,   1460,     24,
      -700,   -534,  -1482,  -1274,  -1889,  -1462,  -1165,    498,
    },
	.y_ws = {
     47530,  45827,  47329,  46543,  47500,  39233,  36210,  30622,
      7527,  -1956,  -3861,  -6565,  -8850,  -2663,  17811,  30337,
     32955,  35625,  36183,  34297,  31111,  22736,  11936,  11390,
     11127,   8611,   7075,   5431,   4133,   1025,   -230,  -3173,
    },
	.z_ws = {
     65536,  60579,  57180,  50562,  42620,  33835,  27760,  17647,
      -441, -17623, -30836, -44245, -52466, -57851, -58433, -57739,
    -49391, -43390, -33711, -25982, -20303, -14384,  -2741,  -1191,
      4131,   8108,   9231,   8688,   7429,   7087,   3718,   1772,
    },
  },
 {
	/*** Y-FILTER 5 - Y-Trig, last ditch ***/
	.lower_ths = -12426520,
	.lt_action = accept,
	.upper_ths = -12426520,
	.gt_action = reject,
	.x_ws = {
       875,   1455,   2967,   3329,   4117,   4681,   4213,   5223,
     11094,   9426,  10778,  11242,   8419,   6770,   4561,    181,
     -3241,  -2474,  -1254,   -589,     58,  -1959,  -1772,   -898,
     -1634,  -2112,  -3274,  -4059,  -6223,  -4204,  -5376,  -4537,
    },
	.y_ws = {
     65536,  61232,  56366,  51409,  51652,  46910,  42081,  44920,
     32305,  33810,  31785,  27682,  13671,  -2106,  -4544, -20085,
    -32984, -34875, -24709, -20044, -17095, -15706, -14056, -16639,
    -21309, -24412, -27448, -26347, -27058, -22404, -26025, -26796,
    },
	.z_ws = {
     49577,  47392,  44007,  38492,  30898,  26335,  27750,  19467,
      8738,  -5727, -15988, -23553, -24545, -27792, -31910, -36103,
    -34737, -30514, -25906, -27430, -25064, -22688, -16886, -15474,
    -13882, -11034, -10079, -10972,  -9973, -11085, -18889, -20353,
    },
  },
};

const macc_fltr_t syh_fltrs[] = 
{
 {
	/*** SUPER Y-FILTER 1 - LD 2axis, from 16 samples ***/
	.lower_ths = 32641288,
	.lt_action = reject,
	.upper_ths = 44881771,
	.gt_action = reject,
	.x_ws = {
     -8210,  -8060,  -8955,  -8971,  -9368,  -9104, -10555,  -9811,
    -10049, -10429, -10590, -11387, -12805, -10727, -11444, -12130,
    -12990, -13794, -17153, -16694, -15913, -16785, -17062, -17802,
    -18755, -18824, -18977, -18345, -17704, -17354, -17663, -16980,
    },
	.y_ws = {
     24225,  26623,  27654,  28450,  29231,  30294,  32617,  32675,
     34207,  33634,  34097,  34245,  34645,  36627,  37661,  37829,
     38961,  39407,  41753,  42194,  48230,  51651,  54675,  58717,
     61467,  64221,  65536,  65514,  64524,  63357,  63046,  59882,
    },
	.z_ws = {
      7585,   6706,   5865,   5378,   5563,   4845,   6663,   6186,
      6431,   5698,   4814,   4903,   6510,   3455,   3700,   2270,
      2088,   1274,   1300,    740,   1613,   1415,   1265,   1533,
      1315,   1900,   2425,   2983,   2239,   2168,   2888,   2996,
    },
  },
};



//___ I N T E R R U P T S  ___________________________________________________
static void accel_isr(void) {
    if (!accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &click_flags.b8)) {
        DISP_ERR_CONSEC_READ_1();
    }
    if (!accel_register_consecutive_read(AX_REG_INT1_SRC, 1, &int1_flags.b8)) {
        DISP_ERR_CONSEC_READ_2();
    }
    if (!accel_register_consecutive_read(AX_REG_INT2_SRC, 1, &int2_flags.b8)) {
        DISP_ERR_CONSEC_READ_3();
    }

#if ( DEBUG_AX_ISR )
    if ( click_flags.ia ) _led_on_full( 56 );
    if ( int1_flags.xl )  _led_on_full( 14 );   //      y
    if ( int1_flags.xh )  _led_on_full( 44 );   //      ^
    if ( int1_flags.yl )  _led_on_full( 59 );   //      |
    if ( int1_flags.yh )  _led_on_full( 29 );   //      |
    if ( int1_flags.zl )  _led_on_full(  7 );   //      .---> x
    if ( int1_flags.zh )  _led_on_full( 36 );   //     /
    if ( int2_flags.xl )  _led_on_full( 16 );   //    /
    if ( int2_flags.xh )  _led_on_full( 46 );   //  z'
    if ( int2_flags.yl )  _led_on_full(  1 );
    if ( int2_flags.yh )  _led_on_full( 31 );
    if ( int2_flags.zl )  _led_on_full(  9 );
    if ( int2_flags.zh )  _led_on_full( 38 );
    delay_ms(100);
    if ( click_flags.ia ) _led_off_full( 56 );
    if ( int1_flags.xl )  _led_off_full( 14 );
    if ( int1_flags.xh )  _led_off_full( 44 );
    if ( int1_flags.yl )  _led_off_full( 59 );
    if ( int1_flags.yh )  _led_off_full( 29 );
    if ( int1_flags.zl )  _led_off_full(  7 );
    if ( int1_flags.zh )  _led_off_full( 36 );
    if ( int2_flags.xl )  _led_off_full( 16 );
    if ( int2_flags.xh )  _led_off_full( 46 );
    if ( int2_flags.yl )  _led_off_full(  1 );
    if ( int2_flags.yh )  _led_off_full( 31 );
    if ( int2_flags.zl )  _led_off_full(  9 );
    if ( int2_flags.zh )  _led_off_full( 38 );
#endif  /* DEBUG_AX_ISR */

    extint_chan_clear_detected(AX_INT_CHAN);

    /* FIXME : as soon as the interrupt is cleared, the fifo is free to 'stream' again,
     * so we should read the fifo before the interrupt gets cleared!!! (or
     * right afterwareds before another sample is taken)
     * */

    /* Wait for accelerometer to release interrupt */
    uint8_t i = 0;
    while( extint_chan_is_detected(AX_INT_CHAN) ) {
        uint8_t dummy;
        extint_chan_clear_detected(AX_INT_CHAN);
        accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &dummy);
        accel_register_consecutive_read(AX_REG_INT1_SRC, 1, &dummy);
        accel_register_consecutive_read(AX_REG_INT2_SRC, 1, &dummy);

        if (i > 250) {      /* 'i' is expected to roll over */
            /* ### HACK to guard against unreleased AX interrupt */
            accel_register_write (AX_REG_CTL3,  0);
            accel_register_write (AX_REG_CTL3,  I1_CLICK_EN);
            accel_register_write (AX_REG_CLICK_CFG, X_DCLICK );
            accel_register_write (AX_REG_CLICK_THS, SLEEP_CLICK_THS);
            accel_register_write (AX_REG_TIME_LIM, SLEEP_CLICK_TIME_LIM);
            accel_register_write (AX_REG_TIME_LAT, SLEEP_CLICK_TIME_LAT);
            DISP_ERR_ISR_RELEASE()
        }
        i++;
    };
}


//___ F U N C T I O N S ______________________________________________________
static void wait_state_conf( wake_gesture_state_t wait_state ) {
    /* 1 LSb = 32 mg @ FS = 4g
     * NOTE: if we are checking for 'z' high, then this filter will pass
     * when z is positive and the magnitude of 'x' and 'y' is below the
     * threshold value.. In other words, this threshold doesn't check that
     * 'z' is greater than some value, it checks that 'x' & 'y' are below
     * some value (or for 'y' it would check that 'z' & 'x' are below value) */
    /* with threshold at 12 and ZH enabled
     * POS: triggers when z > 12 and mag(xy) < 12
     * AND: triggers when abs(z) > 12
     * OR : triggers when abs(z) > ? (something below 12)
     * */

    /* NOTE: changing DURATION_ODR | THRESHOLD changes wake events signature */
    /* NOTE: for duration -- tested 20-70, #samples is ms/10 + 2
     * .. seems like there are n+1 samples checked, the very last sample
     * doesn't matter (could be above/ could be below)*/

    /* NOTE for 6D (ctrlreg5 AOI_POS): 0 and 1 do not work
     * 5 -- uses 'xymag' <= value
     * 2, 3, 4, 10, 15, 20, 21, 23 -- uses 'xymag' < value
     * 24, 25, 26, 30 -- uses 'z' >= value (50, 6 samples, last is not counted)
     * */
    int16_t x, y, z;
    accel_data_read(&x, &y, &z);
#if (SKIP_WAIT_FOR_DOWN)
    wait_state = WAIT_FOR_UP;
#endif  /* SKIP_WAIT_FOR_DOWN */

    /* Configure interrupt to detect orientation status */
    wake_gesture_state = wait_state;

    /* Write bypass to clear FIFO, must be called before changing settings (p.21) */
    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS);

    if( wait_state == WAIT_FOR_DOWN ) {
        accel_register_write (AX_REG_INT1_THS, 20);
        accel_register_write (AX_REG_INT1_DUR, MS_TO_ODRS(70, SLEEP_SAMPLE_INT));
        if ( z < 0 && y > 0 ) {     /* don't allow 'down' event on z-low */
            accel_register_write (AX_REG_INT1_CFG, AOI_POS | XLIE | XHIE | YLIE);
        } else {
            accel_register_write (AX_REG_INT1_CFG, AOI_POS | XLIE | XHIE | YLIE | ZLIE);
        }

#if (WAKE_ON_SUPER_Y)
        if (y < 10) {
            accel_register_write (AX_REG_INT2_THS, 28);
            accel_register_write (AX_REG_INT2_DUR, MS_TO_ODRS(100, SLEEP_SAMPLE_INT));
            accel_register_write (AX_REG_INT2_CFG, AOI_POS | YHIE );
            accel_register_write (AX_REG_CTL3, I1_CLICK_EN | I1_AOI1_EN | I1_AOI2_EN);
        } else {
            accel_register_write (AX_REG_INT2_THS, 0);
            accel_register_write (AX_REG_INT2_DUR, 0);
            accel_register_write (AX_REG_INT2_CFG, 0);
            accel_register_write (AX_REG_CTL3, I1_CLICK_EN | I1_AOI1_EN);
        }
#else
        accel_register_write (AX_REG_CTL3, I1_CLICK_EN | I1_AOI1_EN);
#endif
    } else { /* WAIT_FOR_UP */
        accel_register_write (AX_REG_INT1_THS, 26);
        accel_register_write (AX_REG_INT1_DUR, MS_TO_ODRS(120, SLEEP_SAMPLE_INT));
        accel_register_write (AX_REG_INT1_CFG, AOI_POS | ZHIE);

        accel_register_write (AX_REG_INT2_THS, 10);
        accel_register_write (AX_REG_INT2_DUR, MS_TO_ODRS(80, SLEEP_SAMPLE_INT));
        accel_register_write (AX_REG_INT2_CFG, AOI_POS | YHIE );

        accel_register_write (AX_REG_CTL3, I1_CLICK_EN | I1_AOI1_EN | I1_AOI2_EN);
    }

    /* Enable stream to FIFO buffer mode */
    accel_register_write (AX_REG_FIFO_CTL, STREAM_TO_FIFO);
}

static bool wake_check( void ) {
    if (click_flags.ia) {
        if (dclick_filter_check()) {
            return true;
        } else {
            /* This DCLICK has been filtered out.  Ensure
             * we our configured correctly for sleep based on
             * current wake gesture state */
            if (accel_wakeup_gesture_enabled) {
                wait_state_conf(wake_gesture_state);
            }
            return false;
        }
    }

    /* dclick interrupt flag was not set but only a dclick
     * interrupt could have occurred */
    if (!accel_wakeup_gesture_enabled) {
        DISP_ERR_WAKE_1();
        return dclick_filter_check();
    }

    /* we got here bc of an interrupt, check that at least one flag exists */
    if (!(int1_flags.ia || int2_flags.ia)) {
        DISP_ERR_WAKE_2();
        /* The accelerometer is in an error state (probably a timing
         * error between interrupt trigger and register read) so just
         * assume the interrupt was for what we expect based on state */
        if (wake_gesture_state == WAIT_FOR_DOWN) {
            wait_state_conf( WAIT_FOR_UP );
            return false;
        } else {
            return true;
        }
    }

    /* if our state was 'down', go forward to 'up' */
    if (wake_gesture_state == WAIT_FOR_DOWN) {
#if (WAKE_ON_SUPER_Y)
        if ( int2_flags.ia ) {
            /* we just got a super y-high isr flag, skip to check gesture */
            int2_flags.super = true;
            if (gesture_filter_check()) {
                return true;
            }

            main_nvm_data.filtered_gestures++;

            wait_state_conf(WAIT_FOR_DOWN);
            return false;
        }
        int2_flags.super = false;
#endif  /* WAKE_ON_SUPER_Y */

        wait_state_conf(WAIT_FOR_UP);
        return false;
    }

    /* we are in WAIT_FOR_UP, run gesture filters to determine wake status */
    if (gesture_filter_check()) {
        return true;
    }
    
    /* gesture has been filtered out, wait for down again */
    main_nvm_data.filtered_gestures++;
    wait_state_conf(WAIT_FOR_DOWN);
    
    return false;
}

static bool check_tilt_down( int16_t x, int16_t y, int16_t z ) {
    const int16_t MAX_TILT = 20;
    const int16_t ALMOST_VERT = 28;
    const int16_t DOWN_FACING = -2;
    return (ABS(x) > MAX_TILT ||                            // x-tilted
            (z <= ALMOST_VERT && y <= DOWN_FACING) ||   // turned out
            (z <= DOWN_FACING && y <= ALMOST_VERT));
}

static inline fltr_result_t calc_macc_fltr_action( const macc_fltr_t *macc_fltr ) {
    uint8_t i;
    int32_t result = 0;
    
    for( i = 0; i < accel_fifo.depth; i++ ) {
        result += ((int32_t) accel_fifo.values[i].x_l) * macc_fltr->x_ws[i];
        result += ((int32_t) accel_fifo.values[i].y_l) * macc_fltr->y_ws[i];
        result += ((int32_t) accel_fifo.values[i].z_l) * macc_fltr->z_ws[i];
    }
    
    if (macc_fltr->lt_action != punt) {
        if (result < macc_fltr->lower_ths ) {
            return macc_fltr->lt_action;
        }
    }
    
    if (macc_fltr->gt_action != punt) {
        if (result > macc_fltr->upper_ths ) {
            return macc_fltr->gt_action;
        }
    }

    return punt;
}

static bool check_tilt_not_viewable( int16_t x, int16_t y, int16_t z ) {
    const int16_t SLIGHTLY_VERT = 5;
    const int16_t DOWN_FACING = -15;
    return ((z <= SLIGHTLY_VERT && y <= DOWN_FACING) ||   // turned out
            (z <= DOWN_FACING && y <= SLIGHTLY_VERT));
}

static inline bool gesture_filter_check( void ) {
    /* Read FIFO to determine if a turn-up gesture occurred */
#if (LOG_ACCEL_GESTURE_FIFO || GESTURE_FILTERS)
    read_accel_fifo();
#endif  /* LOG_ACCEL_GESTURE_FIFO || GESTURE_FILTERS */
#if (REJECT_ALL_GESTURES)
#if (LOG_ACCEL_GESTURE_FIFO)
    log_accel_gesture_fifo();
#endif  /* LOG_ACCEL_GESTURE_FIFO */
    return false;
#endif  /* REJECT_ALL_GESTURES */
#if (!(GESTURE_FILTERS))
    return true;
#endif
    uint8_t i;
    fltr_result_t result;
    if( accel_fifo.depth == 0 ) {
        /* there was some error reading the fifo */
        return true;
    }

    if (check_tilt_down(accel_fifo.values[accel_fifo.depth-1].x_l, 
                        accel_fifo.values[accel_fifo.depth-1].y_l, 
                        accel_fifo.values[accel_fifo.depth-1].z_l)) {
        
        /* current orientation would put us to sleep immediately anyway */
        return false;
    }
    
    
    if (int1_flags.ia && int2_flags.ia) {
        /* Both z-high and y-high interrupts are active (rare) */
        /* Usually a true positive so accept always */
        return true;
    }

    if (int2_flags.super) {
        /* A "super Y" event has occurred */
        /* run filters for wakeup due to z-high */
        for (i=0; i < LENGTH(syh_fltrs); i++) {
            result = calc_macc_fltr_action(syh_fltrs + i);
            
            if (result == accept) {
                _DISP_FILTER_INFO((i+1)*5);
                return true;
            } else if (result == reject) {
                _DISP_FILTER_INFO(60-(i+1)*5);
                return false;
            }
        } 
        /* ###FIXME accept any punted super-y for now */
        return true;
    }
    
    if (int1_flags.ia) {
        /* run filters for wakeup due to z-high */
        for (i=0; i < LENGTH(zh_fltrs); i++) {
            result = calc_macc_fltr_action(zh_fltrs + i);
            
            if (result == accept) {
                _DISP_FILTER_INFO((i+1)*5);
                return true;
            } else if (result == reject) {
                _DISP_FILTER_INFO(60-(i+1)*5);
                return false;
            }
        } 
    } 

    /* run filters for y-high (int2 active) */
    if (int2_flags.ia) {
        /* run filters for wakeup due to z-high */
        for (i=0; i < LENGTH(yh_fltrs); i++) {
            result = calc_macc_fltr_action(yh_fltrs + i);
            
            if (result == accept) {
                _DISP_FILTER_INFO(2 + (i+1)*5);
                return true;
            } else if (result == reject) {
                _DISP_FILTER_INFO(60-2-(i+1)*5);
                return false;
            }
        } 
    } 

    /* ###FIXME accept anything that is punted by all filters for now */
    return true;
}

static inline bool dclick_filter_check( void ) {
    int16_t x, y, z;

    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS);
    accel_data_read(&x, &y, &z);

    if (check_tilt_down(x, y, z)) {
        return false;
    }

    if (z >= 12 || y >= 12) {
        return true;
    } else if (z > 0 && y > 0 && (z*z + y*y) >= 144) {
        return true;
    } else {
        main_nvm_data.filtered_dclicks++;
#if ( SHOW_LED_ON_DCLICK_FAIL )
        _led_on_full(31);
        delay_ms(10);
        _led_off_full(31);
#endif  /* SHOW_LED_ON_DCLICK_FAIL */
    }

    return false;
}

static void configure_interrupt ( void ) {
    /* Configure our accel interrupt 1 to wake us up */
    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_NONE;
    port_pin_set_config(AX_INT_PIN, &pin_conf);

    struct extint_chan_conf eint_chan_conf;
    extint_chan_get_config_defaults(&eint_chan_conf);

    eint_chan_conf.gpio_pin             = AX_INT_EIC;
    eint_chan_conf.gpio_pin_mux         = AX_INT_EIC_MUX;
    eint_chan_conf.gpio_pin_pull        = EXTINT_PULL_NONE;
    /* NOTE: cannot wake from standby with filter or edge detection ... */
    eint_chan_conf.detection_criteria   = EXTINT_DETECT_HIGH;
    eint_chan_conf.filter_input_signal  = false;
    eint_chan_conf.wake_if_sleeping     = true;

    extint_chan_set_config(AX_INT_CHAN, &eint_chan_conf);
}

static void configure_i2c(void) {
    /* Initialize config structure and software module */
    struct i2c_master_config config_i2c_master;
    i2c_master_get_config_defaults(&config_i2c_master);

    /* Change buffer timeout to something longer */
    config_i2c_master.baud_rate = I2C_MASTER_BAUD_RATE_400KHZ;
    config_i2c_master.buffer_timeout = 65535;
    config_i2c_master.pinmux_pad0 = AX_SDA_PAD;
    config_i2c_master.pinmux_pad1 = AX_SCL_PAD;
    config_i2c_master.start_hold_time = I2C_MASTER_START_HOLD_TIME_400NS_800NS;
    config_i2c_master.run_in_standby = false;

    /* Initialize and enable device with config */
    while(i2c_master_init(&i2c_master_instance, SERCOM0, &config_i2c_master) != STATUS_OK);

    i2c_master_enable(&i2c_master_instance);
}

static bool accel_register_consecutive_read (uint8_t start_reg, uint8_t count,
    uint8_t *data_ptr) {

    /* register address is 7 LSBs, MSB indicates consecutive or single read */
    uint8_t write = start_reg | (count > 1 ? 1 << 7 : 0);

    /* Write the register address (SUB) to the accelerometer */
    struct i2c_master_packet packet = {
        .address     = i2c_addr,
        .data_length = 1,
        .data = &write,
        .ten_bit_address = false,
        .high_speed      = false,
        .hs_master_code  = 0x0,
    };

    if ( STATUS_OK != i2c_master_write_packet_wait_no_stop (
                &i2c_master_instance, &packet ) ) {
        return false;
    }

    packet.data = data_ptr;
    packet.data_length = count;

    if (STATUS_OK != i2c_master_read_packet_wait ( &i2c_master_instance, &packet )) {
        return false;
    }

    return true;
}

static inline bool fltr_y_not_deliberate_fail( int16_t y_last,
        accel_xyz_t* sums ) {
    bool y_test = y_last < -5;
    return y_test;
}


static inline bool fltr_y_turn_arm_accept( accel_xyz_t* sums ) {
    /* "Turn Arm Up" filter */
    bool ytest = ABS( sums[9].y ) >= 240;
    return ytest;
}

static inline bool fltr_x_turn_arm_accept( accel_xyz_t* sums ) {
    /* "Turn Arm Up" filter */
    /* when turning arm up from 3 or 9 o'clock down the absolute value
     * sum of the first N x-values should exceed a certain threshold */
    bool xtest = ABS( sums[5].x ) >= 120;
    /*  for xtest, check if sum of first N x-values are large enough in
     *  the negative (or positive for lefties) direction
     *  (3 or 9 o'clock facing down)
     *  */
    return xtest;
}

static inline bool fltr_xy_turn_arm_accept( accel_xyz_t* sums ) {
    /* "Turn Arm Up" filter */
    bool xytest = ABS( sums[9].y ) + ABS( sums[5].x ) > 140;
    return xytest;
}

static inline bool fltr_y_overshoot_accept( accel_xyz_t* sums ) {
    bool test1 = sums[31].y - sums[26].y > 20;
    bool test2 = sums[31].y - sums[22].y > 40;
    return test1 || test2;
}

static inline bool fltr_y_facing_inwards_accept( int16_t y_last ) {
    /* Filter #3 - if the y-value is greater than a certain threshold
     * in positive direction (6 o'clock down, wrist turned in towards body)
     * then we should wakeup */
    bool test = y_last >= 4;
    return test;
}

static bool accel_register_write (uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    /* Write the register address (SUB) to the accelerometer */
    struct i2c_master_packet packet = {
        .address     = i2c_addr,
        .data_length = 2,
        .data = data,
        .ten_bit_address = false,
        .high_speed      = false,
        .hs_master_code  = 0x0,
    };

    if (STATUS_OK != i2c_master_write_packet_wait ( &i2c_master_instance, &packet)) {
        return false;
    }

    return true;
}

#if ( USE_SELF_TEST )
static void run_self_test( void ) {
    int16_t x0, y0, z0;
    int16_t x1, y1, z1;
    uint8_t reg4_default;
    uint8_t status_reg;
    int16_t stest_low = 17;
    int16_t stest_high = 360;
    uint32_t timeout;
    const uint32_t default_to = 0x10;

    uint8_t st_failure;

    accel_register_consecutive_read( AX_REG_CTL4, 1, &reg4_default );

    /* set acceleration resoultion to +/- 2g */
    accel_register_write( AX_REG_CTL4, FS_2G | STEST_NORMAL );

    /* clear fifo data, then read xyz0 */
    accel_data_read( &x0, &y0, &z0 );     /* clear existing data */
    timeout = default_to;
    do {
        accel_register_consecutive_read( AX_STATUS_REG, 1, &status_reg );
    } while( !(status_reg & ZYXDA) && --timeout );
    if( timeout == 0 ) {
        main_terminate_in_error( error_group_accel, ACCEL_ERROR_TIMEOUT );
    }
    accel_data_read( &x0, &y0, &z0 );

    accel_register_write( AX_REG_CTL4, FS_2G | STEST_TEST1 );
    /* test data is obtained after 2 samples in low power and normal mode
     * or 8 samples in high resolution mode */

    /* clear fifo data, then read xyz1 */
    accel_data_read( &x1, &y1, &z1 );
    timeout = default_to;
    do {
        accel_register_consecutive_read( AX_STATUS_REG, 1, &status_reg );
    } while( !(status_reg & ZYXDA) && --timeout );
    if( timeout == 0 ) {
        main_terminate_in_error( error_group_accel, ACCEL_ERROR_TIMEOUT );
    }
    accel_data_read( &x1, &y1, &z1 );
    timeout = default_to;
    do {
        accel_register_consecutive_read( AX_STATUS_REG, 1, &status_reg );
    } while( !(status_reg & ZYXDA) && --timeout );

    if( timeout == 0 ) {
        main_terminate_in_error( error_group_accel, ACCEL_ERROR_TIMEOUT );
    }
    accel_data_read( &x1, &y1, &z1 );

    x1 <<= 10 - BITS_PER_ACCEL_VAL;
    x0 <<= 10 - BITS_PER_ACCEL_VAL;
    y1 <<= 10 - BITS_PER_ACCEL_VAL;
    y0 <<= 10 - BITS_PER_ACCEL_VAL;
    z0 <<= 10 - BITS_PER_ACCEL_VAL;
    z1 <<= 10 - BITS_PER_ACCEL_VAL;

    /* check for failure modes on self test */
    st_failure = 0;
    if ( ABS(x1 - x0) < stest_low ) {             /* fail x self test low */
        st_failure |= (1<<0);
    } else if ( ABS(x1 - x0) > stest_high ) {     /* fail x self test high */
        st_failure |= (1<<1);
    }
    if ( ABS(y1 - y0) < stest_low ) {             /* fail y self test low */
        st_failure |= (1<<2);
    } else if ( ABS(y1 - y0) > stest_high ) {     /* fail y self test high */
        st_failure |= (1<<3);
    }
    if ( ABS(z1 - z0) < stest_low ) {             /* fail z self test low */
        st_failure |= (1<<4);
    } else if ( ABS(z1 - z0) > stest_high ) {     /* fail z self test high */
        st_failure |= (1<<5);
    }

    accel_register_write( AX_REG_CTL4, reg4_default );

    if( st_failure ) {
        main_terminate_in_error( error_group_accel,
                ACCEL_ERROR_SELF_TEST( st_failure ) );
    }
}
#endif      /* USE_SELF_TEST */

#if ( GESTURE_FILTERS || LOG_ACCEL_GESTURE_FIFO )
static void read_accel_fifo( void ) {
    /* Read fifo depth first (should be 0x1f usually ) */
    accel_register_consecutive_read( AX_REG_FIFO_SRC, 1, &accel_fifo.depth );
    /* Note: STM's FSS FIFO Size register reports the size off-by-1
     * (i.e. as an index instead of actual size).  So a value of 31 means
     * there the depth is 32 and so on */
    accel_fifo.depth = 1 + (accel_fifo.depth & FIFO_SIZE);

    if ( !accel_register_consecutive_read( AX_REG_OUT_X_L,
                sizeof(accel_xyz_t) * accel_fifo.depth, accel_fifo.bytes ) ) {
        accel_fifo.depth = 0;
        DISP_ERR_FIFO_READ();
    }
}
#endif  /* GESTURE_FILTERS || LOG_ACCEL_GESTURE_FIFO */


//___ F U N C T I O N S ______________________________________________________
bool accel_wakeup_check( void ) {
    bool wakeup = false;
#ifdef NO_ACCEL
    return true;
#endif

    /* Callback enable is only active when sleeping */
    extint_chan_disable_callback(AX_INT_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
    wakeup = wake_check();
    extint_chan_enable_callback(AX_INT_CHAN, EXTINT_CALLBACK_TYPE_DETECT);

    return wakeup;
}

bool accel_data_read (int16_t *x_ptr, int16_t *y_ptr, int16_t *z_ptr) {
    uint8_t reg_data[6] = {0};
    /* Read the 6 8-bit registers starting with lower 8-bits of X value */
    if (!accel_register_consecutive_read (AX_REG_OUT_X_L, 6, reg_data)) {
        return false; //failure
    }

    *x_ptr = (int16_t)reg_data[0] | (((int16_t)reg_data[1]) << 8);
    *x_ptr = *x_ptr >> (16 - BITS_PER_ACCEL_VAL);
    *y_ptr = (int16_t)reg_data[2] | (((int16_t)reg_data[3]) << 8);
    *y_ptr = *y_ptr >> (16 - BITS_PER_ACCEL_VAL);
    *z_ptr = (int16_t)reg_data[4] | (((int16_t)reg_data[5]) << 8);
    *z_ptr = *z_ptr >> (16 - BITS_PER_ACCEL_VAL);

    return true;
}

void accel_enable ( void ) {
    //i2c_master_enable(&i2c_master_instance);

#ifdef NO_ACCEL
    return;
#endif

    /* Callback enabled only when sleeping */
    extint_chan_disable_callback(AX_INT_CHAN, EXTINT_CALLBACK_TYPE_DETECT);

    accel_register_write (AX_REG_CTL1, (ACTIVE_ODR | X_EN | Y_EN | Z_EN |
             (BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0)));
    accel_register_write (AX_REG_CLICK_CFG, X_SCLICK);
    accel_register_write (AX_REG_CLICK_THS, ACTIVE_CLICK_THS);
    accel_register_write (AX_REG_TIME_LIM, ACTIVE_CLICK_TIME_LIM);
    accel_register_write (AX_REG_TIME_LAT, ACTIVE_CLICK_TIME_LAT);

    /* Enable single and double click detection */
    accel_register_write (AX_REG_CTL3, I1_CLICK_EN);

    /* Disable FIFO mode */
    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS);
}

#if ( LOG_ACCEL_GESTURE_FIFO )
static inline void log_accel_gesture_fifo( void ) {
    if ((LOG_UNCONFIRMED_GESTURES || accel_confirmed) && accel_fifo.depth) {
        uint8_t START_CODE[3] = { 0x77, 0x77, 0x77 };
        uint8_t END_CODE[3] = { 0x7F, 0x7F, 0x7F };
        uint8_t flags[3];
        uint8_t vbatt_relative;
        uint32_t waketicks;
        int32_t timestamp;
        uint8_t values[3*FIFO_MAX_SIZE];
        uint8_t i;

        flags[0] = accel_confirmed ? 0xCC : 0xEE;
        flags[1] = int1_flags.b8;
        flags[2] = int2_flags.b8;
        vbatt_relative = main_get_vbatt_relative();
        waketicks = main_get_waketicks();
        timestamp = aclock_get_timestamp();

        for(i=0; i<accel_fifo.depth; i++) {
            values[3*i+0] = (uint8_t) (accel_fifo.values[i].x_l & 0xFF);
            values[3*i+1] = (uint8_t) (accel_fifo.values[i].y_l & 0xFF);
            values[3*i+2] = (uint8_t) (accel_fifo.values[i].z_l & 0xFF);
        }

        main_log_data (START_CODE, 3, false);
        main_log_data (flags, 3, false);
        main_log_data ((uint8_t *) &timestamp, 4, false);
        main_log_data ((uint8_t *) &waketicks, 4, false);
        main_log_data (&vbatt_relative, 1, false);
        main_log_data (values, 3*accel_fifo.depth, false);
        main_log_data (END_CODE, 3, true);
    }

}
#endif

void accel_sleep ( void ) {
#ifdef NO_ACCEL
    return;
#endif
    /* Reset click counters */
    accel_register_write (AX_REG_CTL1,
            (SLEEP_ODR | X_EN | Y_EN | Z_EN |
             (BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0)));

    /* Configure click parameters */
    /* Only x-axis double clicks should wake us up */
    accel_register_write (AX_REG_CTL3,  I1_CLICK_EN);
    accel_register_write (AX_REG_CLICK_CFG, X_DCLICK);
    accel_register_write (AX_REG_CLICK_THS, SLEEP_CLICK_THS);
    accel_register_write (AX_REG_TIME_LIM, SLEEP_CLICK_TIME_LIM);
    accel_register_write (AX_REG_TIME_LAT, SLEEP_CLICK_TIME_LAT);

    if (accel_wakeup_gesture_enabled) {
        /* Configure interrupt to detect orientation down */
        wait_state_conf( WAIT_FOR_DOWN );
#if (LOG_ACCEL_GESTURE_FIFO)
        log_accel_gesture_fifo();
#endif  /* LOG_ACCEL_GESTURE_FIFO */
    }

    int1_flags.b8 = 0;
    int2_flags.b8 = 0;
    click_flags.b8 = 0;
    accel_slow_click_cnt = slow_click_counter = 0;
    accel_fast_click_cnt = fast_click_counter = 0;
    accel_fifo.depth = 0;

    extint_chan_enable_callback(AX_INT_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
}

static event_flags_t click_timeout_event_check( void ) {
    /* Check for click timeouts */
    if (fast_click_counter > 0 &&
            (main_get_waketime_ms() > last_click_time_ms + FAST_CLICK_WINDOW_MS)) {
        fast_click_counter = 0;
        return EV_FLAG_ACCEL_FAST_CLICK_END;
    }

    if (slow_click_counter > 0 &&
            (main_get_waketime_ms() > last_click_time_ms + SLOW_CLICK_WINDOW_MS)) {
        slow_click_counter = 0;
        return EV_FLAG_ACCEL_SLOW_CLICK_END;
    }
    return EV_FLAG_NONE;
}

void accel_events_clear( void ) {
    fast_click_counter = 0;
    slow_click_counter = 0;

    accel_fast_click_cnt = 0;
    accel_slow_click_cnt = 0;
}

event_flags_t accel_event_flags( void ) {
    event_flags_t ev_flags = EV_FLAG_NONE;
    int16_t x = 0, y = 0, z = 0;
    static bool int_state = false;//keep track of prev interrupt state
    /* these values assume a 4g scale */
    const uint32_t SLEEP_DOWN_DUR_MS = 200;
    const uint32_t SLEEP_NOT_VIEWABLE_DUR_MS = 200;
    
    /* timestamp (based on main tic count) of most recent
     * y down interrupt for entering sleep on z-low
     */
    static bool tilt_down = false;
    static bool tilt_not_viewable = false;
    static uint32_t tilt_down_timeout_ms = 0;
    static uint32_t tilt_not_viewable_timeout_ms = 0;
#ifdef NO_ACCEL
    return ev_flags;
#endif
    ev_flags |= click_timeout_event_check();

    /* Check for turn down event */
    accel_data_read(&x, &y, &z);
    if (check_tilt_down(x, y, z)) {
        if (!tilt_down) {
            tilt_down = true;
            tilt_down_timeout_ms = main_get_waketime_ms() + SLEEP_DOWN_DUR_MS;
        } else if (main_get_waketime_ms() > tilt_down_timeout_ms) {
            /* Check for accel low-z timeout */
            ev_flags |= EV_FLAG_ACCEL_DOWN;
        }
        if (check_tilt_not_viewable(x, y, z)) {
            if (!tilt_not_viewable) {
                tilt_not_viewable = true;
                tilt_not_viewable_timeout_ms = main_get_waketime_ms() + SLEEP_NOT_VIEWABLE_DUR_MS;
            } else if (main_get_waketime_ms() > tilt_not_viewable_timeout_ms) {
                ev_flags |= EV_FLAG_ACCEL_NOT_VIEWABLE;
            }
        } else {
            tilt_not_viewable = false;
        }
    } else {
        tilt_down = false;
    }

    if (port_pin_get_input_level(AX_INT_PIN)) {
        accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &click_flags.b8);

        if (int_state) return ev_flags; //we've already handled this interrupt
        int_state = true;

        if (!click_flags.ia) return ev_flags;

        if (click_flags.sclick) {
            if (click_flags.x) {
                fast_click_counter++;
                slow_click_counter++;

                accel_fast_click_cnt = fast_click_counter;
                accel_slow_click_cnt = slow_click_counter;

                ev_flags |= EV_FLAG_ACCEL_CLICK;
                last_click_time_ms = main_get_waketime_ms();
            }
        }

        click_flags.b8 = 0;
    } else {
        int_state = false;
    }

    return ev_flags;
}

void accel_init ( void ) {
    uint8_t who_it_be;
    uint8_t write_byte, reg_read;
#ifdef NO_ACCEL
    return;
#endif

    configure_i2c();

    delay_ms(5);        /* accel takes 5ms to power up */
    if (!accel_register_consecutive_read (AX_REG_WHO_AM_I, 1, &who_it_be)) {
        /* ID read at first address failed. try other i2c address */
        i2c_addr = AX_ADDRESS1;
        if (!accel_register_consecutive_read (AX_REG_WHO_AM_I, 1, &who_it_be))
            main_terminate_in_error( error_group_accel, ACCEL_ERROR_READ_ID );
    }

    if (who_it_be != WHO_IS_IT) {
        main_terminate_in_error( error_group_accel,
                ACCEL_ERROR_WRONG_ID( who_it_be ) );
    }

    accel_register_write(AX_REG_CTL5, BOOT);
    delay_ms(5);    /* this is a guess.. 5ms is required from full power
                       off to configure, if it wasn't ready then these
                       registers probably shouldn't validate */

    /* Latch interrupts and enable FIFO */
    write_byte = FIFO_EN | LIR_INT1 | LIR_INT2 | D4D_INT2;
    /* Use 4D for interrupt 2 so that Y-HIGH events can be detected at low
     * thresholds (i.e. slightly turned in).  4D allows since the AOI_POS
     * interrupts are only triggered if the axis of interest exceeds the
     * configured threshold AND all other axes are below that threshold.  If
     * 6D is enabled, then the z-axis is included in that check; with 4D the
     * z-axis is not included -- so we have more freedom with the y-high
     * interrupt */
    accel_register_write (AX_REG_CTL5, write_byte);
    accel_register_consecutive_read(AX_REG_CTL5, 1, &reg_read);
    if(reg_read != write_byte) {ACCEL_ERROR_CONFIG(AX_REG_CTL5, reg_read);}

    /* Using 4g mode */
    accel_register_write (AX_REG_CTL4, FS_4G);
    accel_register_consecutive_read(AX_REG_CTL4, 1, &reg_read);
    if(reg_read != FS_4G) {ACCEL_ERROR_CONFIG(AX_REG_CTL4, reg_read);}

    accel_register_write (AX_REG_TIME_WIN, DCLICK_TIME_WIN);
    accel_register_consecutive_read(AX_REG_TIME_WIN, 1, &reg_read);
    if(reg_read != DCLICK_TIME_WIN) {ACCEL_ERROR_CONFIG(AX_REG_TIME_WIN, reg_read);}

    /* Enable High Pass filter for Clicks, not for AOI function */
    write_byte = HPCLICK | HPCF | HPMS_NORM;
    accel_register_write (AX_REG_CTL2, write_byte);
    accel_register_consecutive_read(AX_REG_CTL2, 1, &reg_read);
    if(reg_read != write_byte) {ACCEL_ERROR_CONFIG(AX_REG_CTL2, reg_read);}

    /* Enable sleep-to-wake by setting activity threshold and duration */
    accel_register_write (AX_REG_ACT_THS, DEEP_SLEEP_THS);
    accel_register_consecutive_read(AX_REG_ACT_THS, 1, &reg_read);
    if(reg_read != DEEP_SLEEP_THS) {ACCEL_ERROR_CONFIG(AX_REG_ACT_THS, reg_read);}

    accel_register_write (AX_REG_ACT_DUR, DEEP_SLEEP_DUR);
    accel_register_consecutive_read(AX_REG_ACT_DUR, 1, &reg_read);
    if(reg_read != DEEP_SLEEP_DUR) {ACCEL_ERROR_CONFIG(AX_REG_ACT_DUR, reg_read);}

    accel_enable();

#if ( USE_SELF_TEST )
    run_self_test(  );
#endif  /* USE_SELF_TEST */

    configure_interrupt();

    extint_register_callback(accel_isr, AX_INT_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
}

void accel_set_gesture_enabled( bool enabled ) {
#if (LOG_ACCEL_GESTURE_FIFO)
    if (main_user_data.wake_gestures) {
        accel_confirmed = !(enabled);
        accel_wakeup_gesture_enabled = true;
    } else {
        /* respect main_user_data.wake_gestures setting */
        accel_wakeup_gesture_enabled = false;
    }
#else
    accel_wakeup_gesture_enabled = enabled;
#endif
}
