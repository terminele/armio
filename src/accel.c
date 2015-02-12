/** file:       accel.c
  * modified:   2014-11-03 11:50:33
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"
#include "main.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

#define AX_SDA_PIN      PIN_PA08
#define AX_SDA_PAD      PINMUX_PA08C_SERCOM0_PAD0
#define AX_SCL_PIN      PIN_PA09
#define AX_SCL_PAD      PINMUX_PA09C_SERCOM0_PAD1
#define AX_INT1_PIN     PIN_PA10
#define AX_INT1_EIC     PIN_PA10A_EIC_EXTINT10
#define AX_INT1_EIC_MUX MUX_PA10A_EIC_EXTINT10
#define AX_INT1_CHAN    10
#define AX_ADDRESS 0x18 //0011000
#define DATA_LENGTH 8

#define AX_REG_OUT_X_L      0x28
#define AX_REG_CTL1         0x20
#define AX_REG_CTL2         0x21
#define AX_REG_CTL3         0x22
#define AX_REG_CTL4         0x23
#define AX_REG_CTL5         0x24
#define AX_REG_WHO_AM_I     0x0F
#define WHO_IS_IT           0x33
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
#define Z_EN        0x03
#define LOW_PWR_EN  0x04


/* Data Rates */
#define ODR_OFF     0x00
#define ODR_1HZ     0x10
#define ODR_10HZ    0x20
#define ODR_25HZ    0x30
#define ODR_50HZ    0x40
#define ODR_100HZ   0x50
#define ODR_200HZ   0x60
#define ODR_400HZ   0x70
#define ODR_1620HZ   0x80
#define ODR_5376Z   0x90

/* CTRL_REG2 */
#define HPCLICK     0x04
#define HPCF        0x10

/* CTRL_REG3 */
#define I1_CLICK_EN 0x80

/* CTRL_REG4 */
#define FS_2G       0x00
#define FS_4G       0x10
#define FS_8G       0x20
#define FS_16G      0x30

/* CTRL_REG5 */
#define LIR_INT1    0x08

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
#define SAMPLE_INT_400HZ    (5/2)

#define MS_TO_ODRS(t, sample_int) (t/sample_int)

/* Click configuration constants */
#define CLICK_THS     23 //FIXME -- set for 8G
#define CLICK_TIME_WIN      MS_TO_ODRS(200, SAMPLE_INT_400HZ)
#define CLICK_TIME_LIM      MS_TO_ODRS(30, SAMPLE_INT_400HZ)
#define CLICK_TIME_LAT      MS_TO_ODRS(40, SAMPLE_INT_400HZ) //ms

#define WAKEUP_CLICK_THS     35 //FIXME -- set for 8g
#define WAKEUP_CLICK_TIME_WIN      MS_TO_ODRS(200, SAMPLE_INT_100HZ)
#define WAKEUP_CLICK_TIME_LIM      MS_TO_ODRS(30, SAMPLE_INT_100HZ)
#define WAKEUP_CLICK_TIME_LAT      MS_TO_ODRS(40, SAMPLE_INT_100HZ) //ms

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________

struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;

static bool enabled = false;

struct i2c_master_module i2c_master_instance;
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


//___ I N T E R R U P T S  ___________________________________________________

//___ F U N C T I O N S   ( P R I V A T E ) __________________________________

static void configure_i2c(void);
  /* @brief setup the i2c module to communicate with accelerometer
   * accelerometer
   * @param
   * @retrn none
   */


static bool accel_register_consecutive_read (uint8_t start_reg, uint8_t count, uint8_t *data_ptr);
  /* @brief reads a series of data from consecutive registers from the
   * accelerometer
   * @param start_reg - address of first register to read
   *        count -  # of consecutive registers to read
   *        data  -  pointer to array of data to be filled with reg values
   * @retrn true on success, false on failure
   */

    //___ F U N C T I O N S ______________________________________________________


static void configure_interrupt ( void ) {
  /* Configure our accel interrupt 1 to wake us up */
  struct port_config pin_conf;
  port_get_config_defaults(&pin_conf);
  pin_conf.direction = PORT_PIN_DIR_INPUT;
  pin_conf.input_pull = PORT_PIN_PULL_NONE;
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

static void configure_i2c(void)
{
    /* Initialize config structure and software module */
    struct i2c_master_config config_i2c_master;
    i2c_master_get_config_defaults(&config_i2c_master);

    /* Change buffer timeout to something longer */
    config_i2c_master.buffer_timeout = 65535;
    config_i2c_master.pinmux_pad0 = AX_SDA_PAD;
    config_i2c_master.pinmux_pad1 = AX_SCL_PAD;
    config_i2c_master.start_hold_time = I2C_MASTER_START_HOLD_TIME_400NS_800NS;
    config_i2c_master.run_in_standby = false;

    /* Initialize and enable device with config */
    while(i2c_master_init(&i2c_master_instance, SERCOM0, &config_i2c_master) != STATUS_OK);

    i2c_master_enable(&i2c_master_instance);
}


static bool accel_register_consecutive_read (uint8_t start_reg, uint8_t count, uint8_t *data_ptr){

    /* register address is 7 LSBs, MSB indicates consecutive or single read */
    uint8_t write = start_reg | (count > 1 ? 1 << 7 : 0);

    /* Write the register address (SUB) to the accelerometer */
    struct i2c_master_packet packet = {
            .address     = AX_ADDRESS,
            .data_length = 1,
            .data = &write,
            .ten_bit_address = false,
            .high_speed      = false,
            .hs_master_code  = 0x0,
    };

    if (STATUS_OK != i2c_master_write_packet_wait_no_stop ( &i2c_master_instance, &packet))
        return false;

    packet.data = data_ptr;
    packet.data_length = count;

    if (STATUS_OK != i2c_master_read_packet_wait ( &i2c_master_instance, &packet ))
        return false;

    return true;
}

static bool accel_register_write (uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    /* Write the register address (SUB) to the accelerometer */
    struct i2c_master_packet packet = {
            .address     = AX_ADDRESS,
            .data_length = 2,
            .data = data,
            .ten_bit_address = false,
            .high_speed      = false,
            .hs_master_code  = 0x0,
    };

    if (STATUS_OK != i2c_master_write_packet_wait ( &i2c_master_instance, &packet))
        return false;

    return true;

}

bool accel_data_read (int16_t *x_ptr, int16_t *y_ptr, int16_t *z_ptr) {
    uint8_t reg_data[6] = {0};
    /* Read the 6 8-bit registers starting with lower 8-bits of X value */
    if (!accel_register_consecutive_read (AX_REG_OUT_X_L, 6, reg_data))
        return false; //failure

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

  accel_register_write (AX_REG_CTL1, ODR_400HZ | LOW_PWR_EN | X_EN | Y_EN | Z_EN);
  accel_register_write (AX_REG_CLICK_CFG, Z_DCLICK | Z_SCLICK | \
        /*Y_DCLICK | Y_SCLICK | X_DCLICK |*/ X_SCLICK | X_DCLICK );
  accel_register_write (AX_REG_CLICK_THS, CLICK_THS);

  accel_register_write (AX_REG_TIME_WIN, CLICK_TIME_WIN);
  accel_register_write (AX_REG_TIME_LIM, CLICK_TIME_LIM);
  accel_register_write (AX_REG_TIME_LAT, CLICK_TIME_LAT);

  extint_chan_disable_callback(AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);

  enabled = true;
}

void accel_sleep ( void ) {

  /* Only x-axis double clicks should wake us up */
  accel_register_write (AX_REG_CLICK_CFG, X_DCLICK);
  accel_register_write (AX_REG_CLICK_THS, WAKEUP_CLICK_THS);

  accel_register_write (AX_REG_CTL1, ODR_100HZ | LOW_PWR_EN |  X_EN);

  accel_register_write (AX_REG_TIME_WIN, WAKEUP_CLICK_TIME_WIN);
  accel_register_write (AX_REG_TIME_LIM, WAKEUP_CLICK_TIME_LIM);
  accel_register_write (AX_REG_TIME_LAT, WAKEUP_CLICK_TIME_LAT);

  /* I2C module doesnt disconnect or set SDA pin high
   * in standby. Since there is a hardware pullup on
   * it, set it high so power doesnt get wasted */
  port_pin_set_output_level(AX_SDA_PIN, true);

  extint_chan_enable_callback(AX_INT1_CHAN, EXTINT_CALLBACK_TYPE_DETECT);
  enabled = false;
}

event_flags_t accel_event_flags( void ) {
  event_flags_t ev_flags = EV_FLAG_NONE;
  if (port_pin_get_input_level(AX_INT1_PIN)) {

    accel_register_consecutive_read(AX_REG_CLICK_SRC, 1, &click_flags.b8);

    if (click_flags.dclick) {
      if (click_flags.x)
        ev_flags |= EV_FLAG_ACCEL_DCLICK_X;
      if (click_flags.y)
        ev_flags |= EV_FLAG_ACCEL_DCLICK_Y;
      if (click_flags.z)
        ev_flags |= EV_FLAG_ACCEL_DCLICK_Z;
    } else if (click_flags.sclick) {
      if (click_flags.x)
        ev_flags |= EV_FLAG_ACCEL_SCLICK_X;
      if (click_flags.y)
        ev_flags |= EV_FLAG_ACCEL_SCLICK_Y;
      if (click_flags.z)
        ev_flags |= EV_FLAG_ACCEL_SCLICK_Z;
    }

    click_flags.b8 = 0;

  }

  return ev_flags;
}

void accel_init ( void ) {
    uint8_t who_it_be;

    configure_i2c();
    if (!accel_register_consecutive_read (AX_REG_WHO_AM_I, 1, &who_it_be))
        main_terminate_in_error(ERROR_ACCEL_READ_ID);

    if (who_it_be != WHO_IS_IT)
        main_terminate_in_error(ERROR_ACCEL_BAD_ID);

    if (!accel_register_write (AX_REG_CTL1, ODR_400HZ | LOW_PWR_EN | X_EN |/* Y_EN |*/ Z_EN))
        main_terminate_in_error(ERROR_ACCEL_WRITE_ENABLE);

    /* Set scale */
    if (!accel_register_write (AX_REG_CTL4, FS_8G))
        main_terminate_in_error(ERROR_ACCEL_WRITE_ENABLE);

    /* Latch interrupts */
    if (!accel_register_write (AX_REG_CTL5, LIR_INT1))
        main_terminate_in_error(ERROR_ACCEL_WRITE_ENABLE);

    /* Enable single and double click detection */
    if (!accel_register_write (AX_REG_CTL3, I1_CLICK_EN))
        main_terminate_in_error(ERROR_ACCEL_WRITE_ENABLE);

    /* Enable High Pass filter for Clicks */
    if (!accel_register_write (AX_REG_CTL2, HPCLICK | HPCF))
        main_terminate_in_error(ERROR_ACCEL_WRITE_ENABLE);

    accel_enable();

    configure_interrupt();
}
