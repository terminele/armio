# Copyright (c) 2016 Terminus Electronics International

CSRCS =
CPPFLAGS =
INC_PATH =
PREBUILD_CMD = touch src/aclock.c;
POSTBUILD_CMD =
LDFLAGS =

# Application target name
TARGET_FLASH = bin/armio_flash.elf
TARGET_SRAM = bin/armio_sram.elf

##############################
### SET DEBUGGING DEFAULTS ###
##############################
# Note: override these in make command

store_lifetime_usage=true
#log_vbatt=true
#debug_ax_isr=true
gestures_filters=false
#wake_gestures_user_default=false
#use_wakeup_alarm=true
#log_accel_stream=true
log_accel_gesture_fifo=true
#log_unconfirmed_gestures=false
#always_active=true
#disable_seconds=true
#sparkle=true
#show_sec_always=true
#self_test_accel=true

ifdef debug_accel_isr
    debug_ax_isr=true
    reject_all_gestures=true
    skip_wait_for_down=true
    log_accel_gesture_fifo=true
    gestures_filters=true
    use_interrupt_2=false
endif

ifdef wakeup_alarm_1m
    use_wakeup_alarm=true
    alarm_interval_min=1
endif

ifdef wakeup_alarm_5m
    use_wakeup_alarm=true
    alarm_interval_min=5
endif

ifdef picture_mode
    always_active=true
    disable_seconds=true
endif

ifdef rtc_cal
    CPPFLAGS+=-D RTC_CALIBRATE=true -D TCC_ASYNC=false
    PREBUILD_CMD += touch src/utils.c; touch src/main.c; touch src/conf/conf_clocks.h; touch src/asf/asf.h;
    CSRCS+=src/asf/sam0/drivers/tcc/tcc.c
    INC_PATH+=src/asf/sam0/drivers/tcc
    TARGET_FLASH = bin/armio_rtc_cal_flash.elf
    TARGET_SRAM = bin/armio_rtc_cal_sram.elf
    POSTBUILD_CMD += touch .compiler_flags; 	# ensure remake all
endif


# Path to top level ASF directory relative to this project directory.
PRJ_PATH = .

# Target CPU architecture: cortex-m3, cortex-m4
ARCH = cortex-m0plus

ifndef chip
    chip=samd21e17
    $(info defaulting to samd21e17 target)
endif

ifndef debugger
    debugger=jlink
    $(info defaulting to jlink debugger)
endif

DEBUGGER_CFG=utils/$(debugger).cfg

NVM_STORED_DATA_SIZE=0x100

ifeq ($(chip),samd20)
    PART = samd20e14
    PARTD = __SAMD20E14__
    UCID=samd20
    UCID_SERCOM=samd20
    UCID_CLOCK=samd20
    OCD_PART_CFG = utils/samd20e.cfg
else
ifeq ($(chip),samd21e17)
    PART=samd21e17a
    PARTD = __SAMD21E17A__
    UCID=samd21
    UCID_SERCOM=samd21_r21_d10_d11
    UCID_CLOCK=samd21_r21
    OCD_PART_CFG = utils/samd21e17.cfg
    NVM_MAX_ADDR = 0x1E000 #~122K
    NVM_LOG_SIZE = 0x10000 #64 K
else
ifeq ($(chip),samd21e16)
    PART=samd21e16a
    PARTD = __SAMD21E16A__
    UCID=samd21
    UCID_SERCOM=samd21_r21_d10_d11
    UCID_CLOCK=samd21_r21
    OCD_PART_CFG = utils/samd21e16.cfg
    NVM_MAX_ADDR = 0xEA00 #~60000
    NVM_LOG_SIZE = 0x2000 #16 K
else
$(error chip must be specified as either samd20 or samd21)
endif
endif
endif

#ifeq ($(debugger), atmel-ice)
    INSTALL_CMD = openocd \
	    -f $(DEBUGGER_CFG) \
	    -f $(OCD_PART_CFG) \
	    -c "init" \
	    -c "reset halt" \
	    -c "flash write_image erase $(target)" \
	    -c "verify_image $(target) 0x00000000 elf" \
	    -c "reset run" \
	    -c "shutdown"

    CHIPERASE_CMD = openocd \
	    -f $(DEBUGGER_CFG) \
	    -f $(OCD_PART_CFG) \
	    -c "init" \
	    -c "at91samd chip-erase" \
	    -c "shutdown"

    SSB_CMD = openocd \
	    -f $(DEBUGGER_CFG) \
	    -f $(OCD_PART_CFG) \
	    -c "init" \
	    -c "reset halt" \
	    -c "at91samd set-security enable" \
	    -c "shutdown"

#else
#ifeq ($(debugger), jlink)
#    INSTALL_CMD = JLinkExe utils/$(chip).jlink
#    CHIPERASE_CMD = JLinkExe utils/$(chip)-erase.jlink
#    SSB_CMD = openocd \
#	    -f $(DEBUGGER_CFG) \
#	    -f $(OCD_PART_CFG) \
#	    -c "init" \
#	    -c "reset halt" \
#	    -c "at91samd set-security enable" \
#	    -c "shutdown"
#else
#$(error unuspported debugger)
#endif
#endif

#make bin output directory if it doesnt exist
BUILD_DIR=bin
$(shell mkdir $(BUILD_DIR) 2>/dev/null)


# List of additional C source files.
CSRCS += \
    src/main.c      	       					\
    src/aclock.c					       	\
    src/accel.c						       	\
    src/anim.c						       	\
    src/control.c					       	\
    src/display.c					       	\
    src/leds.c						       	\
    src/utils.c						       	\
    src/asf/common/utils/interrupt/interrupt_sam_nvic.c        	\
    src/asf/common2/services/delay/sam0/systick_counter.c      	\
    src/asf/sam0/drivers/adc/adc.c                      	\
    src/asf/sam0/drivers/events/events.c                      	\
    src/asf/sam0/drivers/extint/extint.c                      	\
    src/asf/sam0/drivers/extint/extint_callback.c             	\
    src/asf/sam0/drivers/nvm/nvm.c		             	\
    src/asf/sam0/drivers/port/port.c                           	\
    src/asf/sam0/drivers/rtc/rtc_calendar.c		       	\
    src/asf/sam0/drivers/rtc/rtc_calendar_interrupt.c 	       	\
    src/asf/sam0/drivers/system/clock/clock_$(UCID_CLOCK)/clock.c     	\
    src/asf/sam0/drivers/system/clock/clock_$(UCID_CLOCK)/gclk.c      	\
    src/asf/sam0/drivers/sercom/sercom.c			\
    src/asf/sam0/drivers/sercom/sercom_interrupt.c		\
    src/asf/sam0/drivers/sercom/i2c/i2c_$(UCID_SERCOM)/i2c_master.c	\
    src/asf/sam0/drivers/system/interrupt/system_interrupt.c   	\
    src/asf/sam0/drivers/system/pinmux/pinmux.c                	\
    src/asf/sam0/drivers/system/system.c                       	\
    src/asf/sam0/drivers/tc/tc.c			       	\
    src/asf/sam0/drivers/tc/tc_interrupt.c		       	\
    src/asf/sam0/drivers/wdt/wdt.c			       	\
    src/asf/sam0/drivers/wdt/wdt_callback.c		       	\
    src/asf/sam0/utils/cmsis/$(UCID)/source/gcc/startup_$(UCID).c \
    src/asf/sam0/utils/cmsis/$(UCID)/source/system_$(UCID).c   	\
    src/asf/sam0/utils/syscalls/gcc/syscalls.c

# List of assembler source files.
ASSRCS =

# List of include paths.
INC_PATH += \
    src/						       \
    src/conf						       \
    src/asf						       \
    src/asf/common/utils                                       \
    src/asf/common2/services/delay                             \
    src/asf/common2/services/delay/sam0                        \
    src/asf/sam0/drivers/adc				       \
    src/asf/sam0/drivers/events				       \
    src/asf/sam0/drivers/extint				       \
    src/asf/sam0/drivers/nvm				       \
    src/asf/sam0/drivers/port                                  \
    src/asf/sam0/drivers/rtc				       \
    src/asf/sam0/drivers/sercom                                \
    src/asf/sam0/drivers/sercom/i2c                            \
    src/asf/sam0/drivers/sercom/i2c/$(UCID_SERCOM)	       \
    src/asf/sam0/drivers/system                                \
    src/asf/sam0/drivers/system/clock                          \
    src/asf/sam0/drivers/system/clock/clock_$(UCID_CLOCK)             \
    src/asf/sam0/drivers/system/interrupt                      \
    src/asf/sam0/drivers/system/interrupt/system_interrupt_$(UCID) \
    src/asf/sam0/drivers/system/pinmux                         \
    src/asf/sam0/drivers/tc	                               \
    src/asf/sam0/drivers/wdt	                               \
    src/asf/sam0/utils                                         \
    src/asf/sam0/utils/cmsis/$(UCID)/include                    \
    src/asf/sam0/utils/cmsis/$(UCID)/source                     \
    src/asf/sam0/utils/header_files                            \
    src/asf/sam0/utils/preprocessor                            \
    src/asf/thirdparty/CMSIS/Include                           \
    src/asf/thirdparty/CMSIS/Lib/GCC 				\
    src/thirdparty

# Additional search paths for libraries.
LIB_PATH =  \
    src/asf/thirdparty/CMSIS/Lib/GCC \
    src/asf/thirdparty/CMSIS/Lib/GCC/ARM \
    lib/

# List of libraries to use during linking.
LIBS = # \
       #arm_cortexM0l_math

# Path relative to top level directory pointing to a linker script.
LINKER_SCRIPT_FLASH = utils/$(PART)_flash.ld
LINKER_SCRIPT_SRAM  = utils/$(PART)_sram.ld

#debug scripts
DEBUG_SCRIPT_FLASH = utils/armio_flash.gdb
DEBUG_SCRIPT_SRAM  = utils/armio_sram.gdb

# Project type parameter: all, sram or flash
PROJECT_TYPE        = flash

# Additional options for debugging. By default the common Makefile.in will
# add -g3.
DBGFLAGS =

# Application optimization used during compilation and linking:
# -O0, -O1, -O2, -O3 or -Os
OPTIMIZATION = -O3

# Extra flags to use when archiving.
ARFLAGS =

# Extra flags to use when assembling.
ASFLAGS =

# Extra flags to use when compiling.
CFLAGS = -mtune=$(ARCH)

_YEAR_=$(shell date '+%-Y')
_MONTH_=$(shell date '+%-m')
_DAY_=$(shell date '+%-d')
_HOUR_=$(shell date '+%-I')
_MIN_=$(shell date '+%-M')
_SEC_=$(shell date '+%-S')
_AMPM_=$(shell date '+%-P')

ifeq ($(_AMPM_), am)
    _PM_=false
else
    _PM_=true
endif

$(info pm is $(_PM_))
$(info $(_YEAR_)-$(_MONTH_)-$(_DAY_)  $(_HOUR_):$(_MIN_):$(_SEC_)s $(_AMPM_))

# Extra flags to use when preprocessing.
#
# Preprocessor symbol definitions
#   To add a definition use the format "-D name[=definition]".
#   To cancel a definition use the format "-U name".
#
# The most relevant symbols to define for the preprocessor are:
TIME_INFO_FLAGS =
TIME_INFO_FLAGS += -D __YEAR__=$(_YEAR_)
TIME_INFO_FLAGS += -D __MONTH__=$(_MONTH_)
TIME_INFO_FLAGS += -D __DAY__=$(_DAY_)
TIME_INFO_FLAGS += -D __HOUR__=$(_HOUR_)
TIME_INFO_FLAGS += -D __MIN__=$(_MIN_)
TIME_INFO_FLAGS += -D __SEC__=$(_SEC_)
TIME_INFO_FLAGS += -D __PM__=$(_PM_)
# TIME_INFO_FLAGS are used for c/cpp/asm building

CPPFLAGS += -D I2C_MASTER_CALLBACK_MODE=false
CPPFLAGS += -D ARM_MATH_CM0=true
CPPFLAGS += -D ADC_CALLBACK_MODE=false
CPPFLAGS += -D RTC_CALENDAR_ASYNC=true
CPPFLAGS += -D EXTINT_CALLBACK_MODE=true
CPPFLAGS += -D EVENTS_INTERRUPT_HOOKS_MODE=false
CPPFLAGS += -D WDT_CALLBACK_MODE=true
CPPFLAGS += -D TC_ASYNC=true
CPPFLAGS += -D SYSTICK_MODE
CPPFLAGS += -D $(PARTD)
CPPFLAGS += -D NVM_MAX_ADDR=$(NVM_MAX_ADDR)
CPPFLAGS += -D ENABLE_LIGHT_SENSE=true
CPPFLAGS += -D ENABLE_VBATT=true

## apply non-default options if they have been set previously or by make abc=1
ifdef debug_ax_isr
CPPFLAGS += -D DEBUG_AX_ISR=$(debug_ax_isr)
endif
ifdef reject_all_gestures
CPPFLAGS += -D REJECT_ALL_GESTURES=$(reject_all_gestures)
endif
ifdef gestures_filters
CPPFLAGS += -D GESTURE_FILTERS=$(gestures_filters)
endif
ifdef store_lifetime_usage
CPPFLAGS += -D STORE_LIFETIME_USAGE=$(store_lifetime_usage)
endif
ifdef log_accel_stream
CPPFLAGS += -D LOG_ACCEL_STREAM_IN_MODE_1=$(log_accel_stream)
endif
ifdef log_accel_gesture_fifo
CPPFLAGS += -D LOG_ACCEL_GESTURE_FIFO=$(log_accel_gesture_fifo)
endif
ifdef log_unconfirmed_gestures
CPPFLAGS += -D LOG_UNCONFIRMED_GESTURES=$(log_unconfirmed_gestures)
endif
ifdef use_wakeup_alarm
CPPFLAGS += -D USE_WAKEUP_ALARM=$(use_wakeup_alarm)
endif
ifdef alarm_interval_min
CPPFLAGS += -D ALARM_INTERVAL_MIN=$(alarm_interval_min)
endif
ifdef always_active
CPPFLAGS += -D ALWAYS_ACTIVE=$(always_active)
endif
ifdef disable_seconds
CPPFLAGS+= -D DISABLE_SECONDS=$(disable_seconds)
endif
ifdef sparkle
CPPFLAGS+= -D SPARKLE_FOREVER_MODE=$(sparkle)
endif
ifdef show_sec_always
CPPFLAGS+= -D SHOW_SEC_ALWAYS=$(show_sec_always)
endif
ifdef self_test_accel
CPPFLAGS+= -D USE_SELF_TEST=$(self_test_accel)
endif
ifdef log_vbatt
CPPFLAGS+= -D LOG_VBATT=$(log_vbatt)
CPPFLAGS+= -D VBATT_LOG_INTERVAL=100
endif
ifdef wake_gestures_user_default
CPPFLAGS+= -D WAKE_GESTURES_USER_DEFAULT=$(wake_gestures_user_default)
endif
ifdef skip_wait_for_down
CPPFLAGS+= -D SKIP_WAIT_FOR_DOWN=$(skip_wait_for_down)
endif
ifdef use_interrupt_2
CPPFLAGS+= -D USE_INTERRUPT_2=$(use_interrupt_2)
endif
