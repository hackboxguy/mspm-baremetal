# MSPM bare-metal platform

Bare-metal C11 platform for TI MSPM0 and MSPM33 MCUs, built with GNU Make and
open-source tooling.  MSPM0C1106 is the first implementation target; MSPM33
support begins only after the MSPM0 platform gates are met.

## Current status

Phase 0 and Phase 1 are complete; the Phase 2 foundation is under way. `blink` configures the
LaunchPad's red LED (`PA0`) and uses a SysTick 1 ms timebase to toggle it every
500 ms. A MAIN-only OpenOCD program-and-verify cycle, reset, SWD liveness
check, and visible blink have been bench-verified on an LP-MSPM0C1106. See the
[bring-up record](docs/bringup_lp_mspm0c1106.md). The portable SPSC ring buffer,
UART0 TX, compile-out `lib_debug`, `lib_buildinfo`, `lib_boot`, `lib_regmap`,
and safe crash-record encoder are complete. The 115200-baud banner, lossless
burst, and overflow-counter evidence has passed. WWDT0 reset, retained
reset-cause capture, a deliberate HardFault, and clean image-identity readback
are bench-verified; power-cycle retention remains to be checked.

## Prerequisites

- Arm GNU Toolchain 13.2.1 (`arm-none-eabi-gcc`)
- GNU Make
- Native GCC with AddressSanitizer and UndefinedBehaviorSanitizer
- clang-format

## Build and test

```sh
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off
make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02
make DEBUG=off test
make DEBUG=off format-check
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off size
make OPENOCD=/path/to/openocd BOARD=lp_mspm0c1106 APP=blink DEBUG=off flash
```

Artifacts are written to `output/<board>/<app>/<variant>/` as ELF, BIN, HEX,
and map files. The ELF is canonically stamped with a fixed flash
[image identity](docs/image_identity.md); `make ... identity-check` verifies
that its BIN and HEX match. See [the accepted implementation plan](docs/mspm-mcu-code-base-plan.md),
[device facts](docs/device_facts.md), and [vendored-header provenance](ti/README.md).
`make clean` removes the selected board/application/variant output; use
`make clean-all` to remove every generated output tree.

## Board programming and debug

With the LP-MSPM0C1106 connected by USB, use a current upstream OpenOCD build
with XDS110 and MSPM0 support:

```sh
OPENOCD=/path/to/openocd tools/openocd/probe_lp_mspm0c1106.sh
```

This check only attaches, halts, and reads MAIN-flash information. To program
the same ELF that the build generated, run `make ... flash` as shown above.
The command verifies its write and resets the MCU. `make ... gdb` starts an
OpenOCD GDB server on port 3333; it does not program the target. See
[tools/README.md](tools/README.md) for exact commands and the safety boundary.

## Safety boundary

The MSPM0C1106 configuration NVM is outside normal application flash. C-series
has no ROM BSL; a field updater would be separately designed flash firmware.
Normal commands declare only MAIN flash and may not erase or write BCR,
NONMAIN, or DATA flash.
