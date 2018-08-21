TOOLS_DIR := tools
ARM_SDK_DIR := $(TOOLS_DIR)/gcc-arm-none-eabi-5_2-2015q4
ARM_SDK_PREFIX := $(ARM_SDK_DIR)/bin/arm-none-eabi-

ifeq ("$(wildcard $(ARM_SDK_PREFIX)gcc*)","")
    $(error **ERROR** ARM-SDK is not in $(ARM_SDK_DIR))
endif

INC :=
INC += inc

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

CFLAGS :=
# XXX -m4
CFLAGS += -mcpu=cortex-m3 -mthumb -fdata-sections -ffunction-sections
CFLAGS += -fomit-frame-pointer -Wall -Os -g3

ifneq ("$(STACK_USAGE)","")
    OBJ_FORCE := FORCE
    CFLAGS += -fstack-usage

else
    CFLAGS += -Werror
endif

CFLAGS += -DHSE_VALUE=8000000

LDFLAGS := -nostartfiles -Wl,-static -Wl,--warn-common
LDFLAGS += -Wl,--fatal-warnings -Wl,--gc-sections
LDFLAGS += -Ttasker.ld

LIBS := -lgcc -lc_nano

all: build/tasker.bin

%.bin: %
	$(ARM_SDK_PREFIX)objcopy -O binary $< $@

build/tasker: $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) $(OBJ) -o $@ -Tmemory.ld $(LDFLAGS) 

clean:
	rm -rf $(BUILD_DIR)

build/%.o: %.c $(OBJ_FORCE)
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

FORCE:
