/** file:       accel.c
  * modified:   2014-11-03 11:50:33
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"
#include "main.h"
#include "leds.h"


//___ M A C R O S   ( P R I V A T E ) ________________________________________


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
#define USE_SELF_TEST       false
#endif

#ifndef
#define DEBUG_AX_ISR false
#endif

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
#define AX_REG_CLICK_CFG    0x38
#define AX_REG_CLICK_SRC    0x39
#define AX_REG_CLICK_THS    0x3A
#define AX_REG_TIME_LIM     0x3B
#define AX_REG_TIME_LAT     0x3C
#define AX_REG_TIME_WIN     0x3D
#define AX_REG_ACT_THS      0x3E
#define AX_REG_ACT_DUR      0x3F

#define BITS_PER_ACCEL_VAL 8

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
#define HPCF        0x10

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

/* STATUS_REG */
#define ZYXDA       0x08

/* FIFO_CTRL_REG */
#define FIFO_BYPASS     0x00
#define FIFO_STREAM     0x80
#define STREAM_TO_FIFO  0xC0

/* FIFO_CTRL_SRC */
#define FIFO_SIZE       0x0F

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
#define CLICK_THS     43 //assumes 4g scale
#define CLICK_TIME_WIN      MS_TO_ODRS(200, ACTIVE_SAMPLE_INT)
#define CLICK_TIME_LIM      MS_TO_ODRS(20, ACTIVE_SAMPLE_INT)
#define CLICK_TIME_LAT      MS_TO_ODRS(40, ACTIVE_SAMPLE_INT) //ms

#define SLEEP_ODR          ODR_100HZ
#define SLEEP_SAMPLE_INT   SAMPLE_INT_100HZ

#define WAKEUP_CLICK_THS     46 //assumes 4g scale
#define WAKEUP_CLICK_TIME_WIN      MS_TO_ODRS(200, SLEEP_SAMPLE_INT)
#define WAKEUP_CLICK_TIME_LIM      MS_TO_ODRS(40, SLEEP_SAMPLE_INT)
#define WAKEUP_CLICK_TIME_LAT      MS_TO_ODRS(40, SLEEP_SAMPLE_INT) //ms

#define MULTI_CLICK_WINDOW_MS 350

#define Z_DOWN_THRESHOLD        8 //assumes 4g scale
#define Z_DOWN_DUR_MS           200

#define Z_SLEEP_DOWN_THRESHOLD      8 //assumes 4g scale
#define Y_SLEEP_DOWN_THRESHOLD      8 //assumes 4g scale

#define Z_DOWN_THRESHOLD_ABS    (Z_DOWN_THRESHOLD < 0 ? -Z_DOWN_THRESHOLD : Z_DOWN_THRESHOLD)
#define Z_DOWN_DUR_ODR          MS_TO_ODRS(Z_DOWN_DUR_MS, SLEEP_SAMPLE_INT)

/* Y_DOWN */
#define XY_DOWN_THRESHOLD        -10 //assumes 4g scale
#define XY_DOWN_DUR_MS           50
//#define Y_DOWN_THRESHOLD        -18 //assumes 4g scale
//#define Y_DOWN_DUR_MS           200

#define XY_DOWN_THRESHOLD_ABS    (XY_DOWN_THRESHOLD < 0 ? -XY_DOWN_THRESHOLD : XY_DOWN_THRESHOLD)
#define XY_DOWN_DUR_ODR          MS_TO_ODRS(XY_DOWN_DUR_MS, SLEEP_SAMPLE_INT)

#define Z_UP_THRESHOLD          18
#define Z_UP_DUR_ODR            MS_TO_ODRS(150, SLEEP_SAMPLE_INT)
#define TURN_GESTURE_TIMEOUT_S  5

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________
static void configure_i2c(void);
  /* @brief setup the i2c module to communicate with accelerometer
   * accelerometer
   * @param
   * @retrn none
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

static void run_self_test( void );
  /* @brief run the self test on the acceleration module
   *        expect an output change of between 17 and 360, defined by
   *        OUTPUT[LSb](self-test enabled) - OUTPUT[LSb](self-test disabled)
   *        where 1LSB=4mg at 10bit representation and +/- 2g full scale
   *        sign is defined in CTRL_REG4 (0x23)
   * @param None
   * @retrn None
   */


//___ V A R I A B L E S ______________________________________________________

static uint16_t i2c_addr = AX_ADDRESS0;

/* timestamp of most recent y down interrupt */
static bool accel_down = false;
static int32_t accel_down_start_ms = 0;

static struct {
  uint32_t x,y,z;
} last_click_time_ms;

static struct {
    uint32_t x,y,z;
} click_count;

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

static union {
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
} int_flags;

static enum {SLEEP_START = 0, WAIT_FOR_DOWN, WAIT_FOR_UP, WAKE_DCLICK, WAKE_TURN_UP} wake_gesture_state;


//___ I N T E R R U P T S  ___________________________________________________

static void accel_isr(void);

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________
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

  accel_register_write (AX_REG_INT1_THS, Z_UP_THRESHOLD);
  accel_register_write (AX_REG_INT1_DUR, Z_UP_DUR_ODR);
  accel_register_write (AX_REG_FIFO_CTL, STREAM_TO_FIFO );
  accel_register_write (AX_REG_INT1_CFG, AOI_POS | ZHIE);
}

static void wait_for_down_conf( void ) {
  /* Configure interrupt to detect orientaiton down (Y LOW) */
  wake_gesture_state = WAIT_FOR_DOWN;
  accel_register_write ( AX_REG_CTL1, ( SLEEP_ODR | X_EN | Y_EN | Z_EN |
        ( BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0 ) ) );

  accel_register_write (AX_REG_INT1_THS, XY_DOWN_THRESHOLD_ABS);
  accel_register_write (AX_REG_INT1_DUR, XY_DOWN_DUR_ODR);
  accel_register_write (AX_REG_FIFO_CTL, STREAM_TO_FIFO );
  accel_register_write (AX_REG_INT1_CFG, AOI_POS | XLIE | YLIE );
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
  system_interrupt_disable_global();

  accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &click_flags.b8);
  accel_register_consecutive_read(AX_REG_INT1_SRC, 1, &int_flags.b8);
#ifdef DEBUG_AX_ISR
  _led_on_full(15);
  delay_ms(50);
  _led_off_full(15);
#endif
  extint_chan_clear_detected(AX_INT1_CHAN);
  system_interrupt_enable_global();

};

static void accel_wakeup_state_refresh(void) {


#ifdef DEBUG_AX_ISR
    _led_on_full(30 + wake_gesture_state);
    delay_ms(50);
    _led_off_full(30 + wake_gesture_state);
#endif

  if (click_flags.ia) {
    wake_gesture_state = WAKE_DCLICK;
    return;
  }

  if (!int_flags.ia) {
    if (!int_flags.yl && !int_flags.xl && !int_flags.zh ) {
      _DISP_ERROR(57);
    } else {
      _DISP_ERROR(56);
    }

    return;

  }

  if (wake_gesture_state == WAIT_FOR_DOWN) {
    if (int_flags.yl || int_flags.xl ) {
      wait_for_up_conf();
    } else {
      _DISP_ERROR(55);
    }
  } else if (wake_gesture_state == WAIT_FOR_UP) {
    if (!int_flags.zh) {
      _DISP_ERROR(54);
      return;
    }

    /* Read FIFO to determine if a turn-up gesture occurred */
    uint8_t fifo_depth;
    int16_t x = 0, y = 0, z = 0;
    int16_t x_p = 0, y_p = 0, z_p = 0;
    int16_t dx = 0, dy = 0, dz = 0;
    int16_t x_sum = 0, y_sum = 0, z_sum = 0;

    accel_register_consecutive_read( AX_REG_FIFO_SRC, 1, &fifo_depth );
    fifo_depth = fifo_depth & FIFO_SIZE;

    /* First read outside loop to initialize prev values for delta calc */
    accel_data_read(&x, &y, &z);
    x_sum = x;
    y_sum = y;
    z_sum = z;

    while (--fifo_depth) {
      x_p = x; y_p = y; z_p = z;
      accel_data_read(&x, &y, &z);
      x_sum+=x;
      y_sum+=y;
      z_sum+=z;

      dx += x - x_p;
      dy += y - y_p;
      dz += z - z_p;

      if ( 0 && (dx + dy > 25)) {
        wake_gesture_state = WAKE_TURN_UP;
        break;
      }

      if ( y_sum + x_sum < -22  ) {
        wake_gesture_state = WAKE_TURN_UP;
        break;
      }

      if ( y_sum < -15 || dy > 20 ) {
        wake_gesture_state = WAKE_TURN_UP;
        break;
      }
    }

      /* Make sure y-down interrupt time was recent enough to
      * constitute a "turn up" gesture */
      //if ( y_sum < -20 || x_sum < -20  ) {
      wake_gesture_state = WAKE_TURN_UP;
      if ( wake_gesture_state == WAKE_TURN_UP) {

        /* Disable AOI INT1 interrupt (leave CLICK enabled) */
        accel_register_write (AX_REG_CTL3,  I1_CLICK_EN);
        accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );

      } else {
        wait_for_down_conf();
      }
  } else {
    _DISP_ERROR(40 + wake_gesture_state);
  }


}

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

//___ F U N C T I O N S ______________________________________________________

bool accel_wakeup_check( void ) {
  int16_t x,y,z = 0;
  bool should_wakeup = false;

#ifdef NO_ACCEL
  return true;
#endif

  system_interrupt_disable_global();

  accel_wakeup_state_refresh();

  if (wake_gesture_state == WAKE_TURN_UP) {
    should_wakeup = true;
  } else if (wake_gesture_state == WAKE_DCLICK) {
    accel_register_write (AX_REG_FIFO_CTL, FIFO_BYPASS );
    accel_data_read(&x, &y, &z);

    /* Dont wakeup on click without z-axis facing somewhat up */
    if (z > Z_UP_THRESHOLD)  {
      should_wakeup = true;
    } else {
      wait_for_up_conf();
      should_wakeup = false;
    }

  }

  system_interrupt_enable_global();
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
  accel_register_write (AX_REG_CLICK_CFG, Z_SCLICK | Y_SCLICK | X_SCLICK);
  accel_register_write (AX_REG_CLICK_THS, CLICK_THS);

  accel_register_write (AX_REG_TIME_WIN, CLICK_TIME_WIN);
  accel_register_write (AX_REG_TIME_LIM, CLICK_TIME_LIM);
  accel_register_write (AX_REG_TIME_LAT, CLICK_TIME_LAT);

  /* Enable single and double click detection */
  accel_register_write (AX_REG_CTL3, I1_CLICK_EN);

  /* Enable High Pass filter for Clicks */
  accel_register_write (AX_REG_CTL2, HPCLICK | HPCF);

  /* Disable FIFO mode */
  accel_register_write (AX_REG_FIFO_CTL,  FIFO_BYPASS );

  /* Latch interrupts; Enable FIFO */
  accel_register_write (AX_REG_CTL5, LIR_INT1 | FIFO_EN );

  /* Disable sleep activity threshold and duration */
  if (!accel_register_write (AX_REG_ACT_THS, 0x00)) {
      main_terminate_in_error( error_group_accel, ACCEL_ERROR_WRITE_EN );
  }

  if (!accel_register_write (AX_REG_ACT_DUR, 0x00)) {
      main_terminate_in_error( error_group_accel, ACCEL_ERROR_WRITE_EN );
  }

}

void accel_sleep ( void ) {
  int16_t x = 0, y = 0, z = 0;

#ifdef NO_ACCEL
  return;
#endif
  accel_data_read(&x, &y, &z);


  accel_register_write ( AX_REG_CTL1,
      ( SLEEP_ODR | X_EN | Y_EN | Z_EN |
        ( BITS_PER_ACCEL_VAL == 8 ? LOW_PWR_EN : 0 ) ) );

  /* Only x-axis double clicks should wake us up */
  accel_register_write (AX_REG_CLICK_CFG, X_DCLICK);
  accel_register_write (AX_REG_CLICK_THS, WAKEUP_CLICK_THS);

  accel_register_write (AX_REG_CTL3,  I1_CLICK_EN | I1_AOI1_EN );

  /* Configure interrupt to detect orientation down (Y LOW) */
  wait_for_down_conf();

  /* Configure click parameters */
  accel_register_write (AX_REG_TIME_WIN, WAKEUP_CLICK_TIME_WIN);
  accel_register_write (AX_REG_TIME_LIM, WAKEUP_CLICK_TIME_LIM);
  accel_register_write (AX_REG_TIME_LAT, WAKEUP_CLICK_TIME_LAT);

  extint_chan_enable_callback(AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);

}

static event_flags_t click_timeout_event_check( void ) {
  event_flags_t ev_flags = EV_FLAG_NONE;

  /* Check for click timeouts */
  ///###FIXME -- duplicated code can be unified with macros
  if ( click_count.x &&
      main_get_waketime_ms() - last_click_time_ms.x > MULTI_CLICK_WINDOW_MS ) {
    switch(click_count.x) {
      case 1:
        ev_flags |= EV_FLAG_ACCEL_SCLICK_X;
        break;
      case 2:
        ev_flags |= EV_FLAG_ACCEL_DCLICK_X;
        break;
      case 3:
        ev_flags |= EV_FLAG_ACCEL_TCLICK_X;
        break;
      case 4:
        ev_flags |= EV_FLAG_ACCEL_QCLICK_X;
        break;
      case 5:
        ev_flags |= EV_FLAG_ACCEL_5CLICK_X;
        break;
      case 6:
        ev_flags |= EV_FLAG_ACCEL_6CLICK_X;
        break;
      case 7:
        ev_flags |= EV_FLAG_ACCEL_7CLICK_X;
        break;
      case 8:
        ev_flags |= EV_FLAG_ACCEL_8CLICK_X;
        break;
      case 9:
        ev_flags |= EV_FLAG_ACCEL_9CLICK_X;
        break;
      default:
        ev_flags |= EV_FLAG_ACCEL_NCLICK_X;
        break;
    }
    click_count.x = 0;
  }

  if ( click_count.y &&
      main_get_waketime_ms() - last_click_time_ms.y > MULTI_CLICK_WINDOW_MS ) {
    switch(click_count.y) {
      case 1:
        ev_flags |= EV_FLAG_ACCEL_SCLICK_Y;
        break;
      case 2:
        ev_flags |= EV_FLAG_ACCEL_DCLICK_Y;
        break;
      case 3:
        ev_flags |= EV_FLAG_ACCEL_TCLICK_Y;
        break;
      case 4:
        ev_flags |= EV_FLAG_ACCEL_QCLICK_Y;
        break;
    }
    click_count.y = 0;
  }

  if ( click_count.z &&
      main_get_waketime_ms() - last_click_time_ms.z > MULTI_CLICK_WINDOW_MS ) {
    switch(click_count.z) {
      case 1:
        ev_flags |= EV_FLAG_ACCEL_SCLICK_Z;
        break;
      case 2:
        ev_flags |= EV_FLAG_ACCEL_DCLICK_Z;
        break;
    }
    click_count.z = 0;
  }

  return ev_flags;
}


event_flags_t accel_event_flags( void ) {
  event_flags_t ev_flags = EV_FLAG_NONE;
  int16_t x = 0, y = 0, z = 0;
  static bool int_state = false;//keep track of prev interrupt state
#ifdef NO_ACCEL
  return ev_flags;
#endif

  ev_flags |= click_timeout_event_check();

  /* Check for Z Low event
   * * A z low event occurs when the device is not flat (z-high) for a
   * * certain period of time
   * */

  accel_data_read(&x, &y, &z);
  if (z <= Z_SLEEP_DOWN_THRESHOLD && y <= Y_SLEEP_DOWN_THRESHOLD) {
    if (!accel_down) {
      accel_down = true;
      accel_down_start_ms = main_get_waketime_ms();
    } else if (main_get_waketime_ms() - accel_down_start_ms > Z_DOWN_DUR_MS) {
      /* Check for accel low-z timeout */
      return EV_FLAG_ACCEL_Z_LOW;
    }
  } else {
    accel_down = false;
  }

  if (port_pin_get_input_level(AX_INT1_PIN)) {
    uint8_t aoi_flags;
    if (int_state) return ev_flags; //we've already handled this interrupt

    int_state = true;
    accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &click_flags.b8);
    accel_register_consecutive_read(AX_REG_INT1_SRC, 1, &aoi_flags);

    if (!click_flags.ia) return ev_flags;
    if (!click_flags.sign) return ev_flags;

#ifdef NOT_NOW
    if (click_flags.dclick) {
      if (click_flags.x)
        ev_flags |= EV_FLAG_ACCEL_DCLICK_X;
      if (click_flags.y)
        ev_flags |= EV_FLAG_ACCEL_DCLICK_Y;
      if (click_flags.z)
        ev_flags |= EV_FLAG_ACCEL_DCLICK_Z;
    } else
#endif
      if (click_flags.sclick) {
        if (click_flags.x) {
          last_click_time_ms.x = main_get_waketime_ms();
          click_count.x++;
        }

        if (click_flags.y) {
          last_click_time_ms.y = main_get_waketime_ms();
          click_count.y++;
        }

        if (click_flags.z) {
          last_click_time_ms.z = main_get_waketime_ms();
          click_count.z++;
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

  if( USE_SELF_TEST ) {
    run_self_test(  );
  }

  configure_interrupt();

  extint_register_callback(accel_isr, AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
}

// vim:shiftwidth=2
