# MSPM bare-metal platform
#
# Usage:
#   make BOARD=lp_mspm0c1106 APP=blink
#   make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02

BOARD ?= lp_mspm0c1106
APP ?= blink
DEBUG ?= off
VERSION ?= 00.00

# Some developer shells export DEBUG=release.  Only the documented make
# command-line setting participates in this project configuration.
ifneq ($(origin DEBUG),command line)
DEBUG := off
endif

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

.PHONY: all clean clean-all format-check gdb info size test

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

size: $(ELF)
	$(SIZE) --format=berkeley $(ELF)

gdb:
	@echo "Debug-server integration is introduced in Phase 1."
	@false

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)

clean-all:
	rm -rf build output tests/build
