TOOLS_DIR := tools
ARM_SDK_DIR := $(TOOLS_DIR)/gcc-arm-none-eabi-5_2-2015q4
ARM_SDK_PREFIX := $(ARM_SDK_DIR)/bin/arm-none-eabi-

ifeq ("$(wildcard $(ARM_SDK_PREFIX)gcc*)","")
    $(error **ERROR** ARM-SDK is not in $(ARM_SDK_DIR))
endif

INC :=
INC += inc
INC += /home/mlyle/newlib/lib/newlib-nano/arm-none-eabi/include/
INC += tools/gcc-arm-none-eabi-6-2017-q2-update/lib/gcc/arm-none-eabi/6.3.1/include/
BUILD_DIR := build

OPENLAGER_LOADER_SRC := $(wildcard loader/*.c)

SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,build/%.o,$(SRC))

OBJ_FORCE :=

ifeq ("$(STACK_USAGE)","")
    CCACHE_BIN := $(shell which ccache 2>/dev/null)
endif

CC := $(CCACHE_BIN) $(ARM_SDK_PREFIX)gcc

CPPFLAGS += $(patsubst %,-I%,$(INC))
#CPPFLAGS += -DSTM32F411xE -DUSE_STDPERIPH_DRIVER

CFLAGS := -nostdinc
# XXX -m4
CFLAGS += -mcpu=cortex-m3 -mthumb -fdata-sections -ffunction-sections
CFLAGS += -fomit-frame-pointer -Wall -Og -g3

CFLAGS += $(CPPFLAGS)

ifneq ("$(STACK_USAGE)","")
    OBJ_FORCE := FORCE
    CFLAGS += -fstack-usage

else
    CFLAGS += -Werror
endif

CFLAGS += -DHSE_VALUE=8000000

LDFLAGS := -nostartfiles -Wl,-static -Wl,--warn-common

LDFLAGS += -nostdlib
LDFLAGS += -L/home/mlyle/newlib/lib/newlib-nano/arm-none-eabi/lib/thumb/v7-m
LDFLAGS += -Ltools/gcc-arm-none-eabi-6-2017-q2-update/lib/gcc/arm-none-eabi/6.3.1/thumb/v6-m/
LDFLAGS += -Wl,--fatal-warnings -Wl,--gc-sections
LDFLAGS += -Tmemory.ld -Ttasker.ld

LIBS := -lc -lgcc

all: build/tasker.bin

%.bin: %
	$(ARM_SDK_PREFIX)objcopy -O binary $< $@

build/tasker: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@ $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

build/%.o: %.c $(OBJ_FORCE)
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

FORCE:
