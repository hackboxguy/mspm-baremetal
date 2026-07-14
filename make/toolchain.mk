# Toolchain policy.  The version is enforced in CI and reported locally.

ifeq ($(origin CC),default)
CC := arm-none-eabi-gcc
endif

ifeq ($(origin OBJCOPY),undefined)
OBJCOPY := arm-none-eabi-objcopy
endif

ifeq ($(origin SIZE),undefined)
SIZE := arm-none-eabi-size
endif
ifeq ($(origin NM),undefined)
NM := arm-none-eabi-nm
endif
TOOLCHAIN_VERSION ?= 13.2.1

CC_VERSION := $(shell $(CC) -dumpfullversion -dumpversion 2>/dev/null)

ifeq ($(strip $(CC_VERSION)),)
$(error Cannot execute $(CC). Install Arm GNU Toolchain $(TOOLCHAIN_VERSION))
endif

ifneq ($(CC_VERSION),$(TOOLCHAIN_VERSION))
ifeq ($(CI),true)
$(error $(CC) is $(CC_VERSION); expected pinned version $(TOOLCHAIN_VERSION))
else
$(warning $(CC) is $(CC_VERSION); pinned version is $(TOOLCHAIN_VERSION))
endif
endif

VERSION_OK := $(shell printf '%s\n' '$(VERSION)' | grep -E '^[0-9][0-9]\.[0-9][0-9]$$' >/dev/null && echo yes)

ifneq ($(VERSION_OK),yes)
$(error VERSION must be two BCD byte pairs, for example 01.02; got '$(VERSION)')
endif

VERSION_MAJOR := $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR := $(word 2,$(subst ., ,$(VERSION)))

WARNINGS := -Wall -Wextra -Werror -Wconversion -Wshadow -Wundef
COMMON_CFLAGS := -std=c11 -ffreestanding -fno-builtin -fno-common \
	-ffunction-sections -fdata-sections -fstack-usage -g3 $(WARNINGS)

ifeq ($(DEBUG),on)
VARIANT := debug
OPTIMIZATION := -O0
VARIANT_CPPFLAGS := -DDEBUG_ENABLED=1
else ifeq ($(DEBUG),off)
VARIANT := release
OPTIMIZATION := -Os
VARIANT_CPPFLAGS := -DNDEBUG
else
$(error DEBUG must be 'on' or 'off'; got '$(DEBUG)')
endif
