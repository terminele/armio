#
# Copyright (c) 2011 Atmel Corporation. All rights reserved.
#
# \asf_license_start
#
# \page License
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. The name of Atmel may not be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# 4. This software may only be redistributed and used in connection with an
#    Atmel microcontroller product.
#
# THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
# EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# \asf_license_stop
#

# Path to top level ASF directory relative to this project directory.
PRJ_PATH = .

# Target CPU architecture: cortex-m3, cortex-m4
ARCH = cortex-m0plus

ifndef chip
    chip=samd21e17
    $(info defaulting to samd21e17 target)
endif

ifndef debugger
    debugger=atmel-ice
    $(info defaulting to atmel-ice debugger)
endif

DEBUGGER_CFG=utils/$(debugger).cfg

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

# Application target name. Given with suffix .a for library and .elf for a
# standalone application.
TARGET_FLASH = bin/armio_flash.elf
TARGET_SRAM = bin/armio_sram.elf

ifeq ($(debugger), atmel-ice)
    INSTALL_CMD = openocd \
	    -f $(DEBUGGER_CFG) \
	    -f $(OCD_PART_CFG) \
	    -c "init" \
	    -c "reset halt" \
	    -c "flash write_image erase $(target)" \
	    -c "verify_image $(target) 0x00000000 elf" \
	    -c "reset run" \
	    -c "shutdown"
else
ifeq ($(debugger), jlink)
    INSTALL_CMD = JLinkExe utils/$(chip).jlink
else
$(error unuspported debugger)
endif
endif

CHIPERASE_CMD = openocd \
	    -f $(DEBUGGER_CFG) \
	    -f $(OCD_PART_CFG) \
	    -c "init" \
	    -c "at91samd chip-erase" \
	    -c "shutdown"
#make bin output directory if it doesnt exist
BUILD_DIR=bin
$(shell mkdir $(BUILD_DIR) 2>/dev/null)


ifndef test
    CSRCS=src/main.c
else
    CSRCS=src/test/$(test).c
    $(info running with main from $(CSRCS))
endif

# List of additional C source files.
CSRCS += \
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
    src/asf/sam0/utils/cmsis/$(UCID)/source/gcc/startup_$(UCID).c \
    src/asf/sam0/utils/cmsis/$(UCID)/source/system_$(UCID).c   	\
    src/asf/sam0/utils/syscalls/gcc/syscalls.c

# List of assembler source files.
ASSRCS =

# List of include paths.
INC_PATH = \
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

$(info $(_YEAR_)-$(_MONTH_)-$(_DAY_)  $(_HOUR_):$(_MIN_):$(_SEC_)s)

# Extra flags to use when preprocessing.
#
# Preprocessor symbol definitions
#   To add a definition use the format "-D name[=definition]".
#   To cancel a definition use the format "-U name".
#
# The most relevant symbols to define for the preprocessor are:

CPPFLAGS = \
       -D I2C_MASTER_CALLBACK_MODE=false		  \
       -D ARM_MATH_CM0=true                               \
       -D ADC_CALLBACK_MODE=false		     	  \
       -D RTC_CALENDAR_ASYNC=true 		          \
       -D EXTINT_CALLBACK_MODE=true		          \
       -D EVENTS_INTERRUPT_HOOKS_MODE=false               \
       -D TC_ASYNC=true		 		          \
       -D SYSTICK_MODE                                    \
       -D $(PARTD)					  \
       -D NVM_MAX_ADDR=$(NVM_MAX_ADDR)					  \
       -D VBATT_NO_AVERAGE=false			  \
       -D ENABLE_LIGHT_SENSE=false			  \
       -D LOG_USAGE=false				  \
       -D __YEAR__=$(_YEAR_)				  \
       -D __MONTH__=$(_MONTH_)				  \
       -D __DAY__=$(_DAY_)				  \
       -D __HOUR__=$(_HOUR_)				  \
       -D __MIN__=$(_MIN_)				  \
       -D __SEC__=$(_SEC_)				  \
       #-D SIMPLE_TIME_MODE
       #-D USE_WAKEUP_ALARM				  \
       #-D NO_ACCEL
       #-D LOG_ACCEL					  \
       #-D ENABLE_BUTTON				  	  \

ifdef simple_time
    CPPFLAGS+= -D SIMPLE_TIME_MODE
endif

# Extra flags to use when linking
LDFLAGS =

# Pre- and post-build commands
PREBUILD_CMD = touch src/aclock.c;
ifdef test
    if [-a bin/src/main.o]; \
	then \
	    PREBUILD_CMD += rm bin/src/main.o; \
	fi;
else
    if [-a bin/src/test/*.o]; \
	then \
	    PREBUILD_CMD += rm bin/src/test/*.o; \
	fi;
endif

POSTBUILD_CMD =
