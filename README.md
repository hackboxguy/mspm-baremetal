# MSPM bare-metal platform

Bare-metal C11 platform for TI MSPM0 and MSPM33 MCUs, built with GNU Make and
open-source tooling.  MSPM0C1106 is the first implementation target; MSPM33
support begins only after the MSPM0 platform gates are met.

## Current status

Phase 0 is complete and Phase 1 bring-up is under way. `blink` now configures
the LaunchPad's red LED (`PA0`) and uses a SysTick 1 ms timebase to toggle it
every 500 ms.  The image is ready for a **read-only probe check**; it is not
yet a supported `make flash` workflow until MAIN-only program/readback has been
bench-verified.

## Prerequisites

- Arm GNU Toolchain 13.2.1 (`arm-none-eabi-gcc`)
- GNU Make
- Native GCC with AddressSanitizer and UndefinedBehaviorSanitizer
- clang-format

## Build and test

```sh
make BOARD=lp_mspm0c1106 APP=blink
make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02
make test
make format-check
make BOARD=lp_mspm0c1106 APP=blink size
```

Artifacts are written to `output/<board>/<app>/<variant>/` as ELF, BIN, HEX,
and map files.  See [the accepted implementation plan](docs/mspm-mcu-code-base-plan.md),
[device facts](docs/device_facts.md), and [vendored-header provenance](ti/README.md).

## First board check

With the LP-MSPM0C1106 connected by USB, use a current upstream OpenOCD build
with XDS110 and MSPM0 support:

```sh
OPENOCD=/path/to/openocd tools/openocd/probe_lp_mspm0c1106.sh
```

This check only attaches, halts, and reads MAIN-flash information. It does not
program, erase, factory-reset, or declare NONMAIN/BCR/BSL regions. See
[tools/README.md](tools/README.md) for the safety boundary.

## Safety boundary

The MSPM0C1106 BCR/BSL configuration NVM is outside normal application flash.
No normal build or future flash command may erase or write it.
