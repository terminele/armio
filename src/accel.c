/** file:       accel.c
  * modified:   2014-11-03 11:50:33
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"
#include "main.h"
#include "leds.h"


//___ M A C R O S   ( P R I V A T E ) ________________________________________
#define ACCEL_ERROR_TIMEOUT             ((uint32_t) 1)
#define ACCEL_ERROR_SELF_TEST(test)    (((uint32_t) 2) \
    | (((uint32_t) test)<<8))
#define ACCEL_ERROR_WRITE_EN            ((uint32_t) 3)
#define ACCEL_ERROR_READ_ID             ((uint32_t) 4)
#define ACCEL_ERROR_WRONG_ID(id)       (((uint32_t) 5) \
    | (((uint32_t) id)<<8) )

#ifndef ABS
#define ABS(a)       ( a < 0 ? -1 * a : a )
#endif

#ifndef USE_SELF_TEST
# define USE_SELF_TEST       false
#endif

#ifndef DEBUG_AX_ISR
# define DEBUG_AX_ISR false
#endif

#ifndef GESTURE_FILTERS
# define GESTURE_FILTERS    true
#endif

#if ( DEBUG_AX_ISR )
#define _DISP_INFO( i )  do { \
      _led_on_full( i ); \
      delay_ms(100); \
      _led_off_full( i ); \
      delay_ms(50); \
      _led_on_full( i ); \
      delay_ms(100); \
      _led_off_full( i ); \
      delay_ms(50); \
    } while(0);
#else   /* DEBUG_AX_ISR */
#define _DISP_INFO( i )
#endif  /* DEBUG_AX_ISR */

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

#define DISP_ERR_FIFO_READ()            _DISP_ERROR( 30 )
#define DISP_ERR_CONSEC_READ_1()        _DISP_ERROR( 57 )
#define DISP_ERR_CONSEC_READ_2()        _DISP_ERROR( 58 )
#define DISP_ERR_CONSEC_READ_3()        _DISP_ERROR( 59 )
#if ( DEBUG_AX_ISR )
#define DISP_ERR_WAKE_1()               _DISP_ERROR( 19 )
#define DISP_ERR_WAKE_2()               _DISP_ERROR( 56 )
#define DISP_ERR_WAKE_3()               _DISP_ERROR( 55 )
#define DISP_ERR_WAKE_4()               _DISP_ERROR( 54 )
#define DISP_ERR_WAKE_GEST( state )     _DISP_ERROR( state + 40 )
#else   /* DEBUG_AX_ISR */
#define DISP_ERR_WAKE_1()
#define DISP_ERR_WAKE_2()
#define DISP_ERR_WAKE_3()
#define DISP_ERR_WAKE_4()
#define DISP_ERR_WAKE_GEST( state )
#endif /* DEBUG_AX_ISR */

#define AX_SDA_PIN      PIN_PA08
#define AX_SDA_PAD      PINMUX_PA08C_SERCOM0_PAD0
#define AX_SCL_PIN      PIN_PA09
#define AX_SCL_PAD      PINMUX_PA09C_SERCOM0_PAD1
#define AX_INT1_PIN     PIN_PA10
#define AX_INT1_EIC     PIN_PA10A_EIC_EXTINT10
#define AX_INT1_EIC_MUX MUX_PA10A_EIC_EXTINT10
#define AX_INT1_CHAN    10
#define AX_ADDRESS0 0x18 //0011000
#define AX_ADDRESS1 0x19 //0011001
#define DATA_LENGTH 8

#define AX_REG_OUT_X_L      0x28
#define AX_REG_CTL1         0x20
#define AX_REG_CTL2         0x21
#define AX_REG_CTL3         0x22
#define AX_REG_CTL4         0x23
#define AX_REG_CTL5         0x24
#define AX_REG_CTL6         0x25
#define AX_STATUS_REG       0x27
#define AX_REG_FIFO_CTL     0x2E
#define AX_REG_FIFO_SRC     0x2F
#define AX_REG_WHO_AM_I     0x0F
#define WHO_IS_IT           0x33
#define AX_REG_INT1_CFG     0x30
#define AX_REG_INT1_SRC     0x31
#define AX_REG_INT1_THS     0x32
#define AX_REG_INT1_DUR     0x33
#define AX_REG_INT2_CFG     0x34
#define AX_REG_INT2_SRC     0x35
#define AX_REG_INT2_THS     0x36
#define AX_REG_INT2_DUR     0x37
#define AX_REG_CLICK_CFG    0x38
#define AX_REG_CLICK_SRC    0x39
#define AX_REG_CLICK_THS    0x3A
#define AX_REG_TIME_LIM     0x3B
#define AX_REG_TIME_LAT     0x3C
#define AX_REG_TIME_WIN     0x3D
#define AX_REG_ACT_THS      0x3E
#define AX_REG_ACT_DUR      0x3F

/* CTRL_REG1 */
#define X_EN        0x01
#define Y_EN        0x02
#define Z_EN        0x04
#define LOW_PWR_EN  0x08

/* Data Rates */
#define ODR_OFF     0x00
#define ODR_1HZ     0x10
#define ODR_10HZ    0x20
#define ODR_25HZ    0x30
#define ODR_50HZ    0x40
#define ODR_100HZ   0x50
#define ODR_200HZ   0x60
#define ODR_400HZ   0x70
#define ODR_1620HZ  0x80
#define ODR_5376Z   0x90

/* CTRL_REG2 */
#define HPCLICK     0x04
#define HPCF        0x00
#define HPMS_NORM   0x10

/* CTRL_REG3 */
#define I1_CLICK_EN 0x80
#define I1_AOI1_EN  0x40
#define I1_AOI2_EN  0x20

/* CTRL_REG4 */
#define FS_2G       0x00
#define FS_4G       0x10
#define FS_8G       0x20
#define FS_16G      0x30
#define STEST_TEST1     0x04
#define STEST_TEST0     0x02
#define STEST_NORMAL    0x00

/* CTRL_REG5 */
#define BOOT        0x80
#define FIFO_EN     0x40
#define LIR_INT1    0x08
#define LIR_INT2    0x02

/* STATUS_REG */
#define ZYXDA       0x08

/* FIFO_CTRL_REG */
#define FIFO_BYPASS     0x00
#define FIFO_STREAM     0x80
#define STREAM_TO_FIFO  0xC0

/* FIFO_CTRL_SRC */
#define FIFO_SIZE       0x1F

/* INT1/2 CFG */
#define AOI_MOV     0x40
#define AOI_POS     0xC0
#define AOI_AND     0x80
#define ZHIE        0x20
#define ZLIE        0x10
#define YHIE        0x08
#define YLIE        0x04
#define XHIE        0x02
#define XLIE        0x01

/* CLICK_CFG */
#define Z_DCLICK   0x20
#define Z_SCLICK   0x10
#define Y_DCLICK   0x08
#define Y_SCLICK   0x04
#define X_DCLICK   0x02
#define X_SCLICK   0x01

/* CLICK_SRC */
#define INT_EN     0x40
#define DCLICK_EN  0x20
#define SCLICK_EN  0x10
#define CLICK_NEG  0x08
#define CLICK_Z    0x04
#define CLICK_Y    0x02
#define CLICK_X    0x01


#define SAMPLE_INT_50HZ     20
#define SAMPLE_INT_100HZ    10
#define SAMPLE_INT_200HZ    5
#define SAMPLE_INT_400HZ    (5/2)

#define MS_TO_ODRS(t, sample_int) (t/sample_int)

/* Click configuration constants */

#define ACTIVE_ODR              ODR_400HZ
#define ACTIVE_SAMPLE_INT       SAMPLE_INT_400HZ
#define CLICK_THS       55 //assumes 4g scale
#define CLICK_TIME_WIN      MS_TO_ODRS(200, ACTIVE_SAMPLE_INT)
#define CLICK_TIME_LIM      MS_TO_ODRS(20, ACTIVE_SAMPLE_INT)
#define CLICK_TIME_LAT      MS_TO_ODRS(40, ACTIVE_SAMPLE_INT) //ms

#define SLEEP_ODR          ODR_100HZ
#define SLEEP_SAMPLE_INT   SAMPLE_INT_100HZ

/* parameters for ST sleep-to-wake/return-to-sleep functionality */
#define DEEP_SLEEP_THS      2
#define DEEP_SLEEP_DUR       MS_TO_ODRS(1000, SLEEP_SAMPLE_INT)

#define WAKEUP_CLICK_THS     47 //assumes 4g scale
#define WAKEUP_CLICK_TIME_WIN      MS_TO_ODRS(400, SLEEP_SAMPLE_INT)
#define WAKEUP_CLICK_TIME_LIM      MS_TO_ODRS(30, SLEEP_SAMPLE_INT)
#define WAKEUP_CLICK_TIME_LAT      MS_TO_ODRS(100, SLEEP_SAMPLE_INT) //ms

#define FAST_CLICK_WINDOW_MS 400
#define SLOW_CLICK_WINDOW_MS 1800

#define Z_DOWN_THRESHOLD        2 //assumes 4g scale
#define Z_DOWN_DUR_MS           50
#define Z_DOWN_DUR_ODR          MS_TO_ODRS(Z_DOWN_DUR_MS, SLEEP_SAMPLE_INT)

#define Z_SLEEP_DOWN_THRESHOLD          18  /* assumes 4g scale */
#define Y_SLEEP_DOWNOUT_THRESHOLD       -8  // assumes 4g scale
#define Y_SLEEP_DOWNIN_THRESHOLD        8   // assumes 4g scale
#define Z_SLEEP_DOWN_DUR_MS           200

#define XY_DOWN_THRESHOLD       20  /* we want something in the 1/3 g range
                                       which seems like around 10 would work best
                                       documentation is 'wrong', we got this
                                       value from trial and error with
                                       make flag accel_debug=true */
#define XY_DOWN_DUR_MS              50
//#define Y_DOWN_THRESHOLD        -18 //assumes 4g scale
//#define Y_DOWN_DUR_MS           200

#define XY_DOWN_DUR_ODR          MS_TO_ODRS(XY_DOWN_DUR_MS, SLEEP_SAMPLE_INT)

#define Z_UP_THRESHOLD          20 //assumes 4g scale
#define Z_UP_DUR_ODR            MS_TO_ODRS(130, SLEEP_SAMPLE_INT)

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________
typedef enum { fail=-1, punt=0, pass=1 } fltr_result;

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
static void configure_i2c(void);
    /* @brief setup the i2c module to communicate with accelerometer
     * accelerometer
     * @param
     * @retrn none
     */

static inline bool fltr_run_gesture_filters( void );
    /* @brief read the accel fifo and filter turn to wake gestures
     * @param None
     * @retrn true if the watch should wake up
     */

static inline fltr_result fltr_lda_trial( void );
    /* @brief filter using the results of global linear discriminant analysis
     * @retrn true on fail
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

/* Configuration functions for wakeup gesture recognition */
static void wait_for_down_conf( void );

static void wait_for_up_conf( void );

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

static void read_accel_fifo( void );


//___ V A R I A B L E S ______________________________________________________
accel_fifo_t accel_fifo = { .bytes = { 0 }, .depth = 0 };
bool accel_wakeup_gesture_enabled = true;
uint8_t accel_slow_click_cnt = 0;
uint8_t accel_fast_click_cnt = 0;
static uint8_t slow_click_counter = 0;
static uint8_t fast_click_counter = 0;

static uint16_t i2c_addr = AX_ADDRESS0;

/* timestamp (based on main tic count) of most recent
 * y down interrupt for entering sleep on z-low
 */
static int32_t accel_down_start_ms = 0;
static bool accel_down = false;

static uint32_t last_click_time_ms;

static struct i2c_master_module i2c_master_instance;

static union {
    struct {
        uint8_t x:1;
        uint8_t y:1;
        uint8_t z:1;
        uint8_t sign:1;
        uint8_t sclick:1;
        uint8_t dclick:1;
        uint8_t ia:1;
        uint8_t unused:1;
    };
    uint8_t b8;
} click_flags;

typedef union {
    struct {
        uint8_t xl:1;
        uint8_t xh:1;
        uint8_t yl:1;
        uint8_t yh:1;
        uint8_t zl:1;
        uint8_t zh:1;
        uint8_t ia:1;
        uint8_t unused:1;
    };
    uint8_t b8;
} int_reg_flags_t;

static int_reg_flags_t int1_flags;
static int_reg_flags_t int2_flags;

static enum {SLEEP_START = 0, WAIT_FOR_DOWN, WAIT_FOR_UP, WAKE_DCLICK, WAKE_TURN_UP} wake_gesture_state;


//___ I N T E R R U P T S  ___________________________________________________
static void accel_isr(void);


//___ F U N C T I O N S ______________________________________________________
static void configure_interrupt ( void ) {
    /* Configure our accel interrupt 1 to wake us up */
    struct port_config pin_conf;
    port_get_config_defaults(&pin_conf);
    pin_conf.direction = PORT_PIN_DIR_INPUT;
    pin_conf.input_pull = PORT_PIN_PULL_DOWN;
    port_pin_set_config(AX_INT1_PIN, &pin_conf);

    struct extint_chan_conf eint_chan_conf;
    extint_chan_get_config_defaults(&eint_chan_conf);

    eint_chan_conf.gpio_pin             = AX_INT1_EIC;
    eint_chan_conf.gpio_pin_mux         = AX_INT1_EIC_MUX;
    eint_chan_conf.gpio_pin_pull        = EXTINT_PULL_NONE;
    /* NOTE: cannot wake from standby with filter or edge detection ... */
    eint_chan_conf.detection_criteria   = EXTINT_DETECT_HIGH;
    eint_chan_conf.filter_input_signal  = false;
    eint_chan_conf.wake_if_sleeping     = true;

    extint_chan_set_config(AX_INT1_CHAN, &eint_chan_conf);
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

static void wait_for_up_conf( void ) {
    /* Configure interrupt to detect movement up (Z HIGH) */

    wake_gesture_state = WAIT_FOR_UP;
    accel_register_write ( AX_REG_CTL1,
            ( SLEEP_ODR | X_EN | Y_EN | Z_EN |
              ( BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0 ) ) );


    /* Clear FIFO by writing bypass */
    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );

    /* Enable stream to FIFO buffer mode */
    accel_register_write (AX_REG_FIFO_CTL, STREAM_TO_FIFO );
    accel_register_write (AX_REG_INT1_CFG, AOI_POS | ZHIE | YHIE);
    accel_register_write (AX_REG_INT1_THS, Z_UP_THRESHOLD);
    accel_register_write (AX_REG_INT1_DUR, Z_UP_DUR_ODR);

    accel_register_write (AX_REG_CTL3, I1_CLICK_EN | I1_AOI1_EN );
}

static void wait_for_down_conf( void ) {
    /* Configure interrupt to detect orientation down  XY High/LOW*/
    wake_gesture_state = WAIT_FOR_DOWN;
    accel_register_write ( AX_REG_CTL1, ( SLEEP_ODR | X_EN | Y_EN | Z_EN |
                ( BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0 ) ) );

    /* Clear FIFO by writing bypass */
    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );

    /* Enable stream to FIFO buffer mode */
    accel_register_write (AX_REG_FIFO_CTL, STREAM_TO_FIFO );

    accel_register_write (AX_REG_INT1_THS, XY_DOWN_THRESHOLD);
    accel_register_write (AX_REG_INT1_DUR, XY_DOWN_DUR_ODR);
    accel_register_write (AX_REG_INT1_CFG, AOI_POS | YHIE | XLIE | XHIE | YLIE | YHIE | ZLIE);
    accel_register_write (AX_REG_CTL3, I1_CLICK_EN | I1_AOI1_EN );
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

static inline bool fltr_run_gesture_filters( void ) {
    uint8_t i;
    fltr_result rv;
    accel_xyz_t curr;
    accel_xyz_t csum = { .x = 0, .y = 0, .z = 0 };
    accel_xyz_t csums[32];
    //accel_xyz_t prev = { .x = 0, .y = 0, .z = 0 };
    //accel_xyz_t diff;

    if( accel_fifo.depth == 0 ) {
        /* there was some error reading the fifo */
        return true;
    }

    for( i = 0; i < accel_fifo.depth; i++ ) {
        /* find x, y, z values from raw 'left aligned' fifo data */
        curr.x = accel_fifo.values[i].x_leftalign; /* same as bit shift right */
        curr.y = accel_fifo.values[i].y_leftalign;
        curr.z = accel_fifo.values[i].z_leftalign;

        /* maintain integrated value */
        csum.x += curr.x;
        csum.y += curr.y;
        csum.z += curr.z;

        /* record integrated value */
        csums[i].x = csum.x;
        csums[i].y = csum.y;
        csums[i].z = csum.z;
#if 0
        /* find differential */
        diff.x = curr.x - prev.x;
        diff.y = curr.y - prev.y;
        diff.z = curr.z - prev.z;

        /* reset previous values */
        prev.x = curr.x;
        prev.y = curr.y;
        prev.z = curr.z;

        if ( diff.z + diff.y > 25 ) {
            return true;
        }
#endif
    }

    if( (rv = fltr_lda_trial()) ) {
        return rv == pass ? true : false;
    } else if( fltr_y_turn_arm_accept( csums ) ) {
        _DISP_INFO(0);
        return true;
    } else if( fltr_y_not_deliberate_fail( curr.y, csums ) ) {
        return false;
    } else if( fltr_z_sum_slope_accept( csums ) ) {
        _DISP_INFO(40);
        return true;
    } else if( fltr_x_turn_arm_accept( csums ) ) {
        _DISP_INFO(15);
        return true;
    } else if( fltr_xy_turn_arm_accept( csums ) ) {
        _DISP_INFO(20);
        return true;
    } else if( fltr_y_facing_inwards_accept( curr.y ) ) {
        _DISP_INFO(30);
        return true;
    } else if( fltr_y_overshoot_accept( csums ) ) {
        return true;
    }
    return false;
}

static inline bool fltr_y_not_deliberate_fail( int16_t y_last,
        accel_xyz_t* sums ) {
    bool y_test = y_last < -5;
    return y_test;
}

static inline bool fltr_z_sum_slope_accept( accel_xyz_t* sums ) {
    bool ztest = sums[31].z - sums[31-11].z - sums[11].z >= 110;
    return ztest;
}

static inline fltr_result fltr_lda_trial( void ) {
    uint8_t i;
    float result = 0;
    static const float xfltr[] = {
         0.141933, -0.079084,  0.053980, -0.041508,
         0.123805, -0.078324,  0.007129,  0.046085,
        -0.149527, -0.016936,  0.137592, -0.123492,
         0.095997, -0.032155, -0.090147,  0.085894,
         0.069998, -0.152560,  0.032722,  0.211516,
        -0.234142, -0.126543,  0.065795,  0.139264,
         0.068951, -0.123922,  0.183790, -0.113392,
         0.028607,  0.073242, -0.039289, -0.129341,
        };
    static const float yfltr[] = {
         0.058247, -0.029759,  0.021983,  0.031913,
        -0.056556,  0.084111,  0.016522, -0.066709,
         0.073694, -0.037538,  0.035218, -0.106093,
         0.143102, -0.069969, -0.017433,  0.070129,
        -0.036476, -0.011404, -0.066406,  0.051376,
         0.010802, -0.101228,  0.171893, -0.115520,
        -0.061091,  0.041638,  0.015095, -0.083972,
         0.022753, -0.037038, -0.085245, -0.035515,
        };
    static const float zfltr[] = {
         0.078621, -0.028656,  0.098588,  0.026159,
         0.065912, -0.159825,  0.046649,  0.058219,
         0.039456, -0.061853,  0.005054,  0.050738,
        -0.013076, -0.053478,  0.016502,  0.070044,
         0.024463, -0.305687,  0.286704, -0.005720,
        -0.028155, -0.085459,  0.055251,  0.013130,
         0.025644, -0.000160, -0.051606,  0.215364,
        -0.198540,  0.084835,  0.191784, -0.240035,
        };
    for( i = 0; i < accel_fifo.depth; i++ ) {
        result += ((float) accel_fifo.values[i].x_leftalign) * xfltr[i];
        result += ((float) accel_fifo.values[i].y_leftalign) * yfltr[i];
        result += ((float) accel_fifo.values[i].z_leftalign) * zfltr[i];
    }
    return result <= 0 ? pass : fail;
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
    uint8_t led_info;
    if( click_flags.ia ) {
        _led_on_full(15); //*click_flags.ia);// + 30*int1_flags.ia + 5*int1_flags.zh);
        delay_ms(10);
        _led_off_full(15);//*click_flags.ia);// + 30*int1_flags.ia + 5*int1_flags.zh);
    } else if (int1_flags.ia) {
        led_info = ( /* int1_flags.ia + */
                15*int1_flags.xl + 45*int1_flags.xh +
                 1*int1_flags.yl + 31*int1_flags.yh +
                23*int1_flags.zl + 52*int1_flags.zh );
        _led_on_full( led_info );
        delay_ms(200);
        _led_off_full( led_info );
    } else if (int2_flags.ia) {
        _led_on_full( 30*int2_flags.ia +
                int2_flags.yl + 3*int1_flags.yh +
                5*int2_flags.zl + 10*int1_flags.zh );
        delay_ms(200);
        _led_off_full(30*int2_flags.ia +
                int2_flags.yl + 3*int1_flags.yh +
                5*int2_flags.zl + 10*int1_flags.zh);
    }
#endif  /* DEBUG_AX_ISR */

    extint_chan_clear_detected(AX_INT1_CHAN);

    /* Wait for accelerometer to release interrupt */
    uint8_t i = 0;
    while(extint_chan_is_detected(AX_INT1_CHAN)) {
        uint8_t dummy;
        extint_chan_clear_detected(AX_INT1_CHAN);
        accel_register_consecutive_read(AX_REG_INT1_SRC, 1, &dummy);
        accel_register_consecutive_read(AX_REG_INT2_SRC, 1, &dummy);
        accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &dummy);
        i+=1;

        if (i > 1000) {
            /* ### HACK to guard against unreleased AX interrupt */
            accel_register_write (AX_REG_CTL3,  0);
            accel_register_write (AX_REG_CTL3,  I1_CLICK_EN);
            accel_register_write (AX_REG_CLICK_CFG, X_DCLICK );
            accel_register_write (AX_REG_CLICK_THS, WAKEUP_CLICK_THS);
            accel_register_write (AX_REG_TIME_WIN, WAKEUP_CLICK_TIME_WIN);
            accel_register_write (AX_REG_TIME_LIM, WAKEUP_CLICK_TIME_LIM);
            accel_register_write (AX_REG_TIME_LAT, WAKEUP_CLICK_TIME_LAT);

            _led_on_full(45);
            delay_ms(100);
            _led_off_full(45);
        }
    };
}

static void accel_wakeup_state_refresh(void) {
    if (click_flags.ia) {
        wake_gesture_state = WAKE_DCLICK;
        return;
    }

    if (!accel_wakeup_gesture_enabled) {
        DISP_ERR_WAKE_1();
        /* FIXME -- this means the dclick interrupt flag
         * was not set but the interrupt for it occurred */
        wake_gesture_state = WAKE_DCLICK;
        return;
    }

    if ( !int1_flags.ia && !int2_flags.ia ) {
        DISP_ERR_WAKE_2();

        /* The accelerometer is in an error state (probably a timing
         * error between interrupt trigger and register read) so just
         * assume the interrupt was for what we expect based on state */
        if (!accel_wakeup_gesture_enabled) {
            wake_gesture_state = WAKE_DCLICK;
        } else if (wake_gesture_state == WAIT_FOR_DOWN) {
            wait_for_up_conf();
        } else {
            wake_gesture_state = WAKE_TURN_UP;
        }
        return;
    }

    if (wake_gesture_state == WAIT_FOR_DOWN) {
        if ( int1_flags.yl || int1_flags.xl || int1_flags.yh || int1_flags.xh || int1_flags.zl ) {
            wait_for_up_conf();
        } else {
            DISP_ERR_WAKE_3();
            wait_for_up_conf();
        }
    } else if (wake_gesture_state == WAIT_FOR_UP) {
        if (!int1_flags.zh && !int1_flags.yh) {
            DISP_ERR_WAKE_4();
            wake_gesture_state = WAKE_TURN_UP;
            return;
        }

#if ( GESTURE_FILTERS )
        /* Read FIFO to determine if a turn-up gesture occurred */
        read_accel_fifo();
        if( fltr_run_gesture_filters() ) {
            wake_gesture_state = WAKE_TURN_UP;
        }

#else   /* GESTURE_FILTERS */
# ifdef LOG_FIFO
        read_accel_fifo();
# endif
        wake_gesture_state = WAKE_TURN_UP;
#endif  /* GESTURE_FILTERS */

        if ( wake_gesture_state == WAKE_TURN_UP) {
            /* Disable AOI INT1 interrupt (leave CLICK enabled) */
            accel_register_write (AX_REG_CTL3,  I1_CLICK_EN);
            accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );
        } else {
            /* No filter has accepted -- do not turn on */
            wait_for_down_conf();
        }
    } else {
        /* Unexpected wake_gesture_state */
        DISP_ERR_WAKE_GEST( wake_gesture_state );
        wake_gesture_state = WAKE_TURN_UP;
    }
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

//___ F U N C T I O N S ______________________________________________________
bool accel_wakeup_check( void ) {
    int16_t x,y,z = 0;
    bool should_wakeup = false;

#ifdef NO_ACCEL
    return true;
#endif

    accel_wakeup_state_refresh();

    if (wake_gesture_state == WAKE_TURN_UP) {
        should_wakeup = true;
    } else if (wake_gesture_state == WAKE_DCLICK) {
        accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );
        accel_data_read(&x, &y, &z);
        if (z >= 12) {
            should_wakeup = true;
        } else {
            _led_on_full(31);
            delay_ms(10);
            _led_off_full(31);
        }
    }
    return should_wakeup;
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

    /* Callback enable is only active when sleeping */
    extint_chan_disable_callback(AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);

    accel_register_write (AX_REG_CTL4, FS_4G);
    accel_register_write ( AX_REG_CTL1,
            ( ACTIVE_ODR | X_EN | Y_EN | Z_EN |
              ( BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0 ) ) );
    accel_register_write (AX_REG_CLICK_CFG, X_SCLICK);
    accel_register_write (AX_REG_CLICK_THS, CLICK_THS);

    accel_register_write (AX_REG_TIME_WIN, CLICK_TIME_WIN);
    accel_register_write (AX_REG_TIME_LIM, CLICK_TIME_LIM);
    accel_register_write (AX_REG_TIME_LAT, CLICK_TIME_LAT);

    /* Enable single and double click detection */
    accel_register_write (AX_REG_CTL3, I1_CLICK_EN);

    /* Enable High Pass filter for Clicks */
    accel_register_write (AX_REG_CTL2, HPCLICK | HPCF | HPMS_NORM);

    /* Disable FIFO mode */
    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );

    /* Latch interrupts and enable FIFO */
    accel_register_write (AX_REG_CTL5, LIR_INT1 | FIFO_EN);

    /* Enable sleep-to-wake by setting activity threshold and duration */
    if (!accel_register_write (AX_REG_ACT_THS, DEEP_SLEEP_THS)) {
        main_terminate_in_error( error_group_accel, ACCEL_ERROR_WRITE_EN );
    }

    if (!accel_register_write (AX_REG_ACT_DUR, DEEP_SLEEP_DUR)) {
        main_terminate_in_error( error_group_accel, ACCEL_ERROR_WRITE_EN );
    }
}

void accel_sleep ( void ) {
    int16_t x = 0, y = 0, z = 0;

#ifdef NO_ACCEL
    return;
#endif
    /* Reset click counters */
    accel_slow_click_cnt = slow_click_counter = 0;
    accel_fast_click_cnt = fast_click_counter = 0;
    accel_fifo.depth = 0;

    accel_data_read(&x, &y, &z);

    accel_register_write ( AX_REG_CTL1,
            ( SLEEP_ODR | X_EN | Y_EN | Z_EN |
              ( BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0 ) ) );

    /* Configure click parameters */
    /* Only x-axis double clicks should wake us up */
    accel_register_write (AX_REG_CTL3,  I1_CLICK_EN );
    accel_register_write (AX_REG_CLICK_CFG, X_DCLICK );
    accel_register_write (AX_REG_CLICK_THS, WAKEUP_CLICK_THS);
    accel_register_write (AX_REG_TIME_WIN, WAKEUP_CLICK_TIME_WIN);
    accel_register_write (AX_REG_TIME_LIM, WAKEUP_CLICK_TIME_LIM);
    accel_register_write (AX_REG_TIME_LAT, WAKEUP_CLICK_TIME_LAT);

    if (accel_wakeup_gesture_enabled) {
        /* Configure interrupt to detect orientation down (Y LOW) */
        wait_for_down_conf();
    }

    extint_chan_enable_callback(AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
}

#if 0
static void accel_reset ( void ) {
    accel_register_write (AX_REG_CTL5, BOOT);
    accel_enable();
}
#endif

static event_flags_t click_timeout_event_check( void ) {
    /* Check for click timeouts */
    if (fast_click_counter > 0 &&
            (main_get_waketime_ms() - last_click_time_ms > FAST_CLICK_WINDOW_MS)) {
        fast_click_counter = 0;
        return EV_FLAG_ACCEL_FAST_CLICK_END;
    }

    if (slow_click_counter > 0 &&
            (main_get_waketime_ms() - last_click_time_ms > SLOW_CLICK_WINDOW_MS)) {
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
#ifdef NO_ACCEL
    return ev_flags;
#endif
    ev_flags |= click_timeout_event_check();

    /* Check for turn down event
     * * A turn down event occurs when the device is not flat (z-high) for a
     * * certain period of time and either turned away slightly (y < -1/4g) or
     * turned down almost flat (y < 1/8g )
     * */

    accel_data_read(&x, &y, &z);
    if (z <= Z_SLEEP_DOWN_THRESHOLD &&
            ( y <= Y_SLEEP_DOWNOUT_THRESHOLD || y <= Y_SLEEP_DOWNIN_THRESHOLD )) {
        if (!accel_down) {
            accel_down = true;
            accel_down_start_ms = main_get_waketime_ms();
        } else if (main_get_waketime_ms() - accel_down_start_ms > Z_SLEEP_DOWN_DUR_MS) {
            /* Check for accel low-z timeout */
            ev_flags |= EV_FLAG_ACCEL_DOWN;
        }
    } else {
        accel_down = false;
    }

    if (port_pin_get_input_level(AX_INT1_PIN)) {
        accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &click_flags.b8);

        if (int_state) return ev_flags; //we've already handled this interrupt
        int_state = true;

        if (!click_flags.ia) return ev_flags;
        if (!click_flags.sign) return ev_flags;

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
#ifdef NO_ACCEL
    return;
#endif

    configure_i2c();


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

    accel_enable();

#if ( USE_SELF_TEST )
    run_self_test(  );
#endif  /* USE_SELF_TEST */

    configure_interrupt();

    extint_register_callback(accel_isr, AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
}
