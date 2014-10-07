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

# Target part: none, sam3n4 or sam4l4aa
PART = samd20e14

# Application target name. Given with suffix .a for library and .elf for a
# standalone application.
TARGET_FLASH = bin/armio_flash.elf
TARGET_SRAM = bin/armio_sram.elf

#make bin output directory if it doesnt exist
BUILD_DIR=bin
$(shell mkdir $(BUILD_DIR) 2>/dev/null)


# List of C source files.
CSRCS = \
    src/main.c						       \
    src/leds.c						       \
    src/asf/common/utils/interrupt/interrupt_sam_nvic.c        \
    src/asf/sam0/drivers/port/port.c                           \
    src/asf/sam0/drivers/sercom/sercom.c                       \
    src/asf/sam0/drivers/sercom/sercom_interrupt.c             \
    src/asf/sam0/drivers/sercom/spi/spi.c                      \
    src/asf/sam0/drivers/system/clock/clock_samd20/clock.c     \
    src/asf/sam0/drivers/system/clock/clock_samd20/gclk.c      \
    src/asf/sam0/drivers/system/interrupt/system_interrupt.c   \
    src/asf/sam0/drivers/system/pinmux/pinmux.c                \
    src/asf/sam0/drivers/system/system.c                       \
    src/asf/sam0/utils/cmsis/samd20/source/gcc/startup_samd20.c \
    src/asf/sam0/utils/cmsis/samd20/source/system_samd20.c     \
    src/asf/sam0/utils/syscalls/gcc/syscalls.c

# List of assembler source files.
ASSRCS =

# List of include paths.
INC_PATH = \
    src/						       \
    src/asf						       \
    src/asf/common/utils                                       \
    src/asf/common2/services/delay                             \
    src/asf/common2/services/delay/sam0                        \
    src/asf/sam0/drivers/port                                  \
    src/asf/sam0/drivers/sercom                                \
    src/asf/sam0/drivers/sercom/spi                            \
    src/asf/sam0/drivers/system                                \
    src/asf/sam0/drivers/system/clock                          \
    src/asf/sam0/drivers/system/clock/clock_samd20             \
    src/asf/sam0/drivers/system/interrupt                      \
    src/asf/sam0/drivers/system/interrupt/system_interrupt_samd20 \
    src/asf/sam0/drivers/system/pinmux                         \
    src/asf/sam0/utils                                         \
    src/asf/sam0/utils/cmsis/samd20/include                    \
    src/asf/sam0/utils/cmsis/samd20/source                     \
    src/asf/sam0/utils/header_files                            \
    src/asf/sam0/utils/preprocessor                            \
    src/asf/thirdparty/CMSIS/Include                           \
    src/asf/thirdparty/CMSIS/Lib/GCC \

    #src/asf/common/boards                                      \
    #src/asf/sam0/boards                                        \

# Additional search paths for libraries.
LIB_PATH =  \
    src/asf/thirdparty/CMSIS/Lib/GCC \
    src/asf/thirdparty/CMSIS/Lib/GCC/ARM \
    lib/

# List of libraries to use during linking.
LIBS =  \
       arm_cortexM0l_math

# Path relative to top level directory pointing to a linker script.
LINKER_SCRIPT_FLASH = utils/samd20e14_flash.ld
LINKER_SCRIPT_SRAM  = utils/samd20e14_sram.ld

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
OPTIMIZATION = -O1

# Extra flags to use when archiving.
ARFLAGS =

# Extra flags to use when assembling.
ASFLAGS =

# Extra flags to use when compiling.
CFLAGS =

# Extra flags to use when preprocessing.
#
# Preprocessor symbol definitions
#   To add a definition use the format "-D name[=definition]".
#   To cancel a definition use the format "-U name".
#
# The most relevant symbols to define for the preprocessor are:
CPPFLAGS = \
       -D ARM_MATH_CM0=true                               \
       -D SPI_CALLBACK_MODE=false                         \
       -D SYSTICK_MODE                                    \
       -D __SAMD20E14__

# Extra flags to use when linking
LDFLAGS = \

# Pre- and post-build commands
PREBUILD_CMD =
POSTBUILD_CMD =
