/** file:       accel.c
  * modified:   2014-11-03 11:50:33
  */

//___ I N C L U D E S ________________________________________________________
#include "accel.h"

//___ M A C R O S   ( P R I V A T E ) ________________________________________

#define AX_SDA_PIN      PINMUX_PA08C_SERCOM0_PAD0 //PIN_PA08
#define AX_SCL_PIN      PINMUX_PA09C_SERCOM0_PAD1 //PIN_PA09
#define AX_INT1_PIN     PIN_PA10

#define AX_ADDRESS 0x19 //0011000
#define DATA_LENGTH 8

#define AX_REG_OUT_X_L    0x28
#define AX_REG_CTL1       0x20
#define AX_REG_WHO_AM_I   0x0F
#define WHO_IS_IT         0x33

#define BITS_PER_ACCEL_VAL  10

//___ T Y P E D E F S   ( P R I V A T E ) ____________________________________

//___ P R O T O T Y P E S   ( P R I V A T E ) ________________________________

//___ V A R I A B L E S ______________________________________________________

struct i2c_master_packet wr_packet;
struct i2c_master_packet rd_packet;

struct i2c_master_module i2c_master_instance;

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

static void configure_i2c(void)
{
    /* Initialize config structure and software module */
    struct i2c_master_config config_i2c_master;
    i2c_master_get_config_defaults(&config_i2c_master);

    /* Change buffer timeout to something longer */
    config_i2c_master.buffer_timeout = 65535;
    config_i2c_master.pinmux_pad0 = AX_SDA_PIN;
    config_i2c_master.pinmux_pad1 = AX_SCL_PIN;
    config_i2c_master.start_hold_time = I2C_MASTER_START_HOLD_TIME_400NS_800NS;

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
  //FIXME -- wakeup accelerometer
}

void accel_disable ( void ) {
  //FIXME -- sleep accelerometer
}

bool accel_init ( void ) {
    uint8_t who_it_be;

    configure_i2c();
    if (!accel_register_consecutive_read (AX_REG_WHO_AM_I, 1, &who_it_be))
        return false;

    if (who_it_be != WHO_IS_IT)
        return false;

    if (!accel_register_write (AX_REG_CTL1, 0x7F))
        return false;

    return true;


}

// vim: shiftwidth=2
