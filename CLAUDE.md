# Project context

This is a C11, GNU Make, bare-metal platform for TI MSPM0 and MSPM33 MCUs.
Phase 0 and Phase 1 are complete; the Phase 2 MSPM0C1106 foundation is under
way. The
`lp_mspm0c1106` / `blink` release image has passed a MAIN-only OpenOCD
program-and-verify cycle and visibly blinks the board's PA0 red LED. See
`docs/bringup_lp_mspm0c1106.md` for the precise tested setup and remaining
Phase 1 evidence. The portable `lib_ringbuf` SPSC primitive, UART0 TX, and
compile-out `lib_debug` transport layer are complete. The 115200-baud
backchannel debug-banner, 32-byte lossless burst, and 128-byte overload/drop
counter tests are recorded. WWDT0 reset and reset-cause retention are
bench-proven; deliberate HardFault capture is also proven. Canonical image
identity artifact/read-back comparison is also bench-proven. Power-cycle
retention remains.

`arch/arch_critical.h` owns short PRIMASK critical sections, and `hal_power`
owns the post-PWREN barrier/delay used by GPIO, UART, and WWDT. The crash record
is now format 2, captures its active exception number, and has a bench-verified
HardFault frame snapshot; re-run the deliberate-HardFault test after any future
fault-path change.

Phase 2 has started with host-tested `lib_buildinfo`, `lib_boot`, `lib_regmap`,
and a safe `lib_crash` diagnostics-page encoder. The Cortex-M0+ target link
test references every portable library, including the PRIMASK-backed GCC atomic
ABI and owned freestanding memory helpers required by `lib_regmap`; keep that
test in CI whenever portable code changes. The C1106 target pin pair is I2C1
PB2/PB3, exposed at BoosterPack positions 9/10; the board's I2C pull-up
footprints are DNC. The I2C1 target backend and `i2c_regmap_demo` now build,
with a host-tested register-free transaction engine and explicit C1106 errata
handling: manual ACK plus delayed `SRXDONE` receive, `STXEMPTY` only for a
transmit request, disabled target wakeup, and no `ACTIVE` toggle during
recovery. The 3.3 V, 100 kHz Raspberry Pi fixture at address `0x42` is
bench-proven for positive target read, repeated-start, and current-address
behavior; negative/recovery tests remain open. An initial polling
`hal_i2c_controller` now owns the same I2C1 PB2/PB3 pair in a separate app,
supports up to four-byte write, read, and combined write/repeated-start/read
operations, and has bounded software plus peripheral SCL-low timeout handling.
`pcf8574a_demo` uses a single 3.3 V PCF8574-compatible module as its safe HIL
fixture, writing only `0xFF` before reading. It has bench-proven write, read,
and repeated-start-read at `0x20`; the non-A address is recorded because the
connected module did not answer in the PCF8574A range. See
`docs/i2c_register_map.md` and `docs/i2c_controller_pcf8574a.md` for the
respective contracts.

## Commands

```sh
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off
make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02
make DEBUG=off test
make DEBUG=off format-check
make OPENOCD=/path/to/openocd BOARD=lp_mspm0c1106 APP=blink DEBUG=off flash
```

## Non-negotiable rules

- Apps never include `ti/` headers or access registers directly.
- Keep `lib/` host-testable and register-free.
- Do not add a normal command that erases or writes MSPM0C1106 BCR/BSL
  configuration NVM.
- Do not invent MSPM33 facts, TrustZone policy, a bootloader, or a field update
  protocol before their plan gates are met.
- Update `docs/device_facts.md` before introducing a hardware-dependent
  constant or workflow.
