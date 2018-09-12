#qemu-system-arm -machine lm3s6965evb -nographic -kernel build/tasker -chardev stdio,mux=on,id=a0 -mon chardev=a0,mode=readline -serial chardev:a0 | less ; stty sane

TOOLS_DIR := tools
ARM_SDK_DIR := $(TOOLS_DIR)/gcc-arm-none-eabi-6-2017-q2-update
ARM_SDK_PREFIX := $(ARM_SDK_DIR)/bin/arm-none-eabi-

ifeq ("$(wildcard $(ARM_SDK_PREFIX)gcc*)","")
    $(error **ERROR** ARM-SDK is not in $(ARM_SDK_DIR))
endif

INC :=
INC += inc
INC += usb
BUILD_DIR := build

CFLAGS :=
CFLAGS += -fomit-frame-pointer -Wall -g3 -Os
LIBS :=
LDFLAGS :=
SRC :=

USEAPP ?= 1

ifeq ($(USEAPP),1)
APPPATH := app
include $(APPPATH)/app.mk
else
# XXX Hack to get board.h
INC += app/inc
endif

SRC += $(wildcard *.c)

ifneq ($(LINUX)x,x)
### Linux specific stuff here ###
BUILD_DIR := build.linux
SRC += $(wildcard linux/*.c)
INC += linux/inc
CPPFLAGS += -pthread
LDFLAGS += -pthread
LIBS += -lreadline
else
### EMBEDDED SPECIFIC STUFF HERE ###
CC := $(ARM_SDK_PREFIX)gcc
LDFLAGS += -nostartfiles -Wl,-static -Wl,--warn-common -nostdlib
LDFLAGS += -L/home/mlyle/newlib/lib/newlib-nano/arm-none-eabi/lib/thumb/v7e-m/fpv4-sp/hard
LDFLAGS += -Ltools/gcc-arm-none-eabi-6-2017-q2-update/lib/gcc/arm-none-eabi/6.3.1/thumb/v7e-m/fpv4-sp/hard
#LDFLAGS += -L/home/mlyle/newlib/lib/newlib-nano/arm-none-eabi/lib/thumb/v7e-m
#LDFLAGS += -Ltools/gcc-arm-none-eabi-6-2017-q2-update/lib/gcc/arm-none-eabi/6.3.1/thumb/v7e-m

LDFLAGS += -Wl,--fatal-warnings -Wl,--gc-sections
LDFLAGS += -Tmemory.ld -Ttasker.ld

LIBS += -lc -lgcc

INC += libs/inc/stm32f3xx
INC += libs/inc/usb
INC += libs/inc/cmsis
INC += /home/mlyle/newlib/lib/newlib-nano/arm-none-eabi/include/
INC += tools/gcc-arm-none-eabi-6-2017-q2-update/lib/gcc/arm-none-eabi/6.3.1/include/
INC += arm/inc

SRC += $(wildcard arm/*.c)
SRC += $(wildcard usb/*.c)
SRC += $(wildcard libs/src/usb/*.c)

CFLAGS += -nostdinc
CFLAGS += -mcpu=cortex-m4 -mthumb -fdata-sections -ffunction-sections
CFLAGS += -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CPPFLAGS += -DSTM32F303xC
CPPFLAGS += -DHSE_VALUE=8000000

all: $(BUILD_DIR)/tasker.bin
### END EMBEDDED SPECIFIC STUFF ###
endif

LIBS += -lm

CC := ccache $(CC)

OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))

OBJ_FORCE :=

ifeq ("$(STACK_USAGE)","")
    CCACHE_BIN := $(shell which ccache 2>/dev/null)
endif

CPPFLAGS += $(patsubst %,-DMAINFUNC=%,$(MAIN_FUNC))
CPPFLAGS += $(patsubst %,-I%,$(INC))
CFLAGS += $(CPPFLAGS)

ifneq ("$(STACK_USAGE)","")
    OBJ_FORCE := FORCE
    CFLAGS += -fstack-usage
else
    CFLAGS += -Werror
endif

all: $(BUILD_DIR)/tasker

%.bin: %
	$(ARM_SDK_PREFIX)objcopy -O binary $< $@

$(BUILD_DIR)/tasker: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@ $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/tasker.bin
	dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s 0x08000000 -D $(BUILD_DIR)/tasker.bin

.PHONY: clean flash

$(BUILD_DIR)/%.o: %.c $(OBJ_FORCE) Makefile
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

$(BUILD_DIR)/%.d: %.c
	@mkdir -p $(dir $@)
	@$(CC) -M -MT "$(patsubst %.d,%.o,$@) $@" $(CPPFLAGS) $< > $@

-include $(patsubst %.c,$(BUILD_DIR)/%.d,$(SRC))

FORCE:
