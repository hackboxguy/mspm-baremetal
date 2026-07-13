# MSPM bare-metal platform
#
# Usage:
#   make BOARD=lp_mspm0c1106 APP=blink DEBUG=off
#   make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02

BOARD ?= lp_mspm0c1106
APP ?= blink
VERSION ?= 00.00
OPENOCD ?= openocd
GDB ?= arm-none-eabi-gdb

# DEBUG changes the output variant; accepting an exported value silently can
# build an unexpected image. Set it explicitly on the make command line.
ifneq ($(origin DEBUG),undefined)
ifneq ($(origin DEBUG),command line)
$(error DEBUG from the environment is not accepted; pass DEBUG=on or DEBUG=off on the make command line)
endif
endif
DEBUG ?= off

PROJ_ROOT := $(abspath .)

BOARD_MK := board/$(BOARD)/board.mk
APP_MAIN := app/$(APP)/main.c

ifeq ($(wildcard $(BOARD_MK)),)
$(error Unknown BOARD '$(BOARD)': expected $(BOARD_MK))
endif

ifeq ($(wildcard $(APP_MAIN)),)
$(error Unknown APP '$(APP)': expected $(APP_MAIN))
endif

include $(BOARD_MK)

ifeq ($(strip $(DEVICE)),)
$(error board/$(BOARD)/board.mk must set DEVICE)
endif

include make/toolchain.mk
include make/device_$(DEVICE).mk
include make/rules.mk

.DEFAULT_GOAL := all

.PHONY: all clean clean-all flash format-check gdb info size test

all: $(ELF) $(BIN) $(HEX)

test:
	$(MAKE) -C tests test

format-check:
	clang-format --dry-run -Werror $(FORMAT_SOURCES)

info:
	@echo "BOARD       = $(BOARD)"
	@echo "APP         = $(APP)"
	@echo "DEVICE      = $(DEVICE)"
	@echo "DEBUG       = $(DEBUG)"
	@echo "VERSION     = $(VERSION)"
	@echo "TOOLCHAIN   = $(TOOLCHAIN_VERSION)"
	@echo "BUILD_DIR   = $(BUILD_DIR)"
	@echo "OUTPUT_DIR  = $(OUT_DIR)"
	@echo "ELF         = $(ELF)"
	@echo "OPENOCD     = $(OPENOCD)"
	@echo "OPENOCD_CFG = $(OPENOCD_CONFIG)"

size: $(ELF)
	$(SIZE) --format=berkeley $(ELF)

flash: $(ELF)
	$(OPENOCD) -f $(OPENOCD_CONFIG) \
		-c "program $(abspath $(ELF)) verify reset exit"

gdb: $(ELF)
	@echo "OpenOCD GDB server will listen on :3333; terminate it with Ctrl-C."
	@echo "In a second terminal: $(GDB) $(ELF) -ex 'target extended-remote :3333'"
	$(OPENOCD) -f $(OPENOCD_CONFIG)

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)

clean-all:
	rm -rf build output tests/build
