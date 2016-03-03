/** file:       accel_reg.h
  * modified:   2016-02-28 14:58:28
  * register and pin information for the accel chip
  */

#ifndef __ACCEL_REG_H__
#define __ACCEL_REG_H__

//___ I N C L U D E S ________________________________________________________

//___ M A C R O S ____________________________________________________________
#define AX_SDA_PIN      PIN_PA08
#define AX_SDA_PAD      PINMUX_PA08C_SERCOM0_PAD0
#define AX_SCL_PIN      PIN_PA09
#define AX_SCL_PAD      PINMUX_PA09C_SERCOM0_PAD1
#define AX_INT_PIN      PIN_PA10
#define AX_INT_EIC      PIN_PA10A_EIC_EXTINT10
#define AX_INT_EIC_MUX  MUX_PA10A_EIC_EXTINT10
#define AX_INT_CHAN    10
#define AX_ADDRESS0 0x18        /* 0011000 */
#define AX_ADDRESS1 0x19        /* 0011001 */
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


//___ T Y P E D E F S ________________________________________________________

//___ V A R I A B L E S ______________________________________________________

//___ P R O T O T Y P E S ____________________________________________________


#endif /* end of include guard: __ACCEL_REG_H__ */
