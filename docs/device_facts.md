# Device facts

**Status:** Phase 1 bring-up in progress. The initial MAIN-only flash, reset,
SWD, SysTick, red-LED, and UART0-TX smoke-test evidence was recorded on
2026-07-13.
Entries marked **pending** still need the applicable TRM, errata, and bench
evidence before their dependent work is accepted.

## Sources

| Source | Revision / date | Use |
|---|---|---|
| [MSPM0C1105/MSPM0C1106 datasheet](https://www.ti.com/lit/ds/symlink/mspm0c1106.pdf) | Rev. B, January 2026 | memory map, core frequency, device capabilities |
| [MSPM0 C-Series Technical Reference Manual](https://www.ti.com/lit/ug/slau893c/slau893c.pdf) | Rev. C, July 2025 | clock and peripheral power-enable sequencing |
| [LP-MSPM0C1106 EVM user's guide](https://www.ti.com/lit/ug/slau950a/slau950a.pdf) | Rev. A, July 2025 | reference-board/debug-probe facts |
| TI MSPM0 SDK | `mspm0_sdk_2_10_00_04`, commit `e249e2bd63bcc912176a30a45a6a5dcea951168b` | device-header and linker-script provenance |

## MSPM0C1106 facts used by Phase 0

| Item | Value | Evidence / consequence |
|---|---|---|
| Core | Arm Cortex-M0+, up to 32 MHz | Datasheet; build uses `-mcpu=cortex-m0plus -mthumb`. |
| Flash | 64 KiB, `0x00000000`–`0x0000FFFF` | Datasheet memory map; `linker.ld` owns this region. |
| SRAM | 8 KiB, `0x20000000`–`0x20001FFF` | Datasheet memory map; `linker.ld` reserves a 1 KiB stack at the top. |
| Configuration NVM | 512 bytes, `0x41C00000`–`0x41C001FF`; BCR is first 256 bytes and BSL configuration is second 256 bytes | Datasheet and TI linker-file provenance. Normal build/flash commands must never erase or write this region. |
| NVIC priority bits | 2 (four levels) | TI device header `__NVIC_PRIO_BITS`; Phase 1 must define the priority/nesting table before enabling interrupts. |
| External interrupt slots | 32 | TI device header IRQ enumeration; owned vector table has 16 core plus 32 external slots. |
| Reference board | LP-MSPM0C1106, with onboard XDS110, backchannel UART, two buttons, RGB LED, and red LED | EVM user's guide. |
| First smoke LED | LED1 is active-high `GPIOA` DIO0 (`PA0`, `PINCM1`); it is connected through R4 (470 Ω) to the red LED | EVM user-guide target schematic. `board_init.c` owns this pin and `app/blink` uses it exclusively. |
| Reset/run clock policy | MCLK resets to SYSOSC; Phase 1 explicitly selects SYSOSC with no divider and requests its 32 MHz base frequency. If bounded status polling fails, it requests the 4 MHz SYSOSC fallback. `FLASHWAIT` is intentionally not programmed: the register has no effect while MCLK is SYSOSC- or LFCLK-sourced. | TI SDK `dl_sysctl_mspm0c1105_c1106.h`, C-Series TRM, and device register header; this is encoded in `hal_clock`. |
| Peripheral power enable | After setting `PWREN.ENABLE` with its key, software waits at least four ULPCLK cycles before accessing other peripheral registers. `PWREN` readback is not a readiness status. The GPIO HAL uses a fixed 64-CPU-cycle startup barrier after the clock HAL's 32 MHz or 4 MHz selection. | C-Series TRM, peripheral power-enable description; applies to future UART, I2C, and WWDT HALs as well. |
| Reset cause | `SYSCTL.SOCLOCK.RSTCAUSE.ID` is a read-to-clear field that reports the lowest-level reset cause since the previous read. Startup reads and stores it before `.data`/`.bss` initialization. `0x0E` is a WWDT0 violation. | C1105/C1106 TI device header and SDK `DL_SYSCTL_getResetCause`; recorded bench evidence below. |
| WWDT0 | WWDT0 is clocked from the default 32.768 kHz LFCLK. Phase 1 uses divide-by-1, a `2^15` count, and zero closed window: nominally 1 second. It continues through WFI but freezes while SWD halts the core. | TI C-series SDK WWDT configuration documentation and device header; encoded by `hal_wdt`. |
| Header provenance | TI BSD-3-Clause device headers and Apache-2.0 CMSIS subset | `ti/README.md`; the vendored source is pinned above. |

## Bench evidence

| Date | Setup and command | Result |
|---|---|---|
| 2026-07-13 | LP-MSPM0C1106 (board revision not recorded), onboard XDS110 firmware `3.0.0.36`, hardware `0x0028`, WSL2 host; OpenOCD `0.12.0+dev-gcb52502` from upstream commit `cb52502e88832386610add9781030f5380344063`; release `blink`, version `01.02`, source commit `ae38d56` | SWD detected the Cortex-M0+ and factory registers reported 64 KiB MAIN flash, one bank, 8 KiB SRAM, and no data flash. `program ... verify reset exit` completed with `Verified OK`; after reset, the application PC was in `main()` and the operator observed the active-high PA0 red LED blinking. See `bringup_lp_mspm0c1106.md`. |
| 2026-07-13 | Same LP-MSPM0C1106/XDS110 and WSL2 host; uncommitted debug `blink`, `VERSION=01.02`; `/dev/ttyACM0` configured for 115200 8N1 before `make OPENOCD=/tmp/openocd-source/src/openocd BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02 flash` | OpenOCD returned `Verified OK` and the XDS110 application/user UART captured `mspm-baremetal: blink`. This verifies the UART0 TX PB6 backchannel path. |
| 2026-07-13 | Same board/tool chain; updated uncommitted debug `blink`. After programming, OpenOCD temporarily set `WWDT0.PDBGCTL.FREE` then left the halted core for 1.5 seconds. A reset-halted read of `g_crash_record` at `0x2000007C` returned magic `0x43525348`, format/size `0x00200001`, reset cause `0x0000000E`, reason `0`, and a valid CRC. | A deliberate stopped-core watchdog expiry caused a reset and startup retained the C1106 WWDT0 reset cause in `.noinit`. This proves the watchdog/reset-cause path; true power-cycle retention remains pending. |
| 2026-07-13 | Same board/tool chain; debug `fault_record_test`, `VERSION=01.02`, programmed with the normal MAIN-only command. The test executes one `udf #0`, then runs normally after its fault reset. A reset-halted read of `g_crash_record` at `0x2000007C` returned `43525348 00200001 00000001 0000001d 00000002 00000140 01000000 34a71d15`. | UART reported the retained hard-fault path. The valid record has sequence 1, CPU-software reset cause `0x1D`, HardFault reason 2, stacked PC `0x140`, stacked xPSR `0x01000000`, and a valid CRC. |
| 2026-07-14 | Same board/tool chain; updated debug `fault_record_test`, `VERSION=01.02`. After one deliberate `udf #0`, a reset-halted read of `g_crash_record` at `0x2000007C` returned `43525348 00200002 00000001 0000001d 00000302 00000174 01000000 f4dc8fbb`. | The format-2 record is valid: sequence 1, CPU-software reset cause `0x1D`, packed HardFault reason 2 plus active IPSR 3 (`0x00000302`), the pre-switch stacked `udf` PC `0x174`, stacked xPSR `0x01000000`, and CRC. This validates the hardened frame snapshot. |
| 2026-07-13 | Same board/tool chain; uncommitted release `blink`, `VERSION=01.02`, source ID `ac2a5153c91c`, dirty flag set. The canonical ELF was MAIN-only programmed and verified, then `make ... identity-readback` read 64 bytes from `0x0000FFC0` and compared them with the ELF. | The ELF/BIN/HEX artifact checker passed and the target read-back exactly matched the format-1 identity: source ID `ac2a5153c91c`, flags `0x0001`, span `0x10000`, CRC32 `0x5ebe198c`. |
| 2026-07-13 | Same board/tool chain; debug `uart_tx_test`, `VERSION=01.02`, with `/dev/ttyACM0` captured at 115200 8N1 before the MAIN-only program-and-verify reset. | The capture contained the complete 32-byte lossless payload, then exactly 64 bytes from the 128-byte overload payload and the `UART_TX_OVERFLOW_OK` marker. That marker requires the board's UART TX drop count to have incremented. |

## Decisions and pending evidence

| Topic | Current rule | Required before |
|---|---|---|
| Flashing | `make ... flash` uses the validated OpenOCD configuration, programs and verifies only MAIN flash, then resets. BCR/BSL, NONMAIN, and DATA flash are not declared. | Extend or replace the tool workflow only after a new scope review. |
| SRAM parity/ECC | `.noinit` records are CRC-gated and were readable after a watchdog reset. **Pending:** true power-cycle retention and any SRAM parity/ECC initialization requirement. | Phase 1 startup/fault record |
| Reset cause | Startup preserves the read-to-clear raw cause in `g_crash_record` before `.data`/`.bss` initialization. WWDT0 cause `0x0E` and software CPU-reset cause `0x1D` after a deliberate HardFault have been bench-verified. **Pending:** cold-boot coverage. | Phase 1 startup/fault record |
| Run clock | SYSOSC 32 MHz configuration and bounded 4 MHz fallback are implemented. A safe-clock fallback produces a 125 ms blink cadence rather than the normal 500 ms cadence. **Pending:** bench timing and the fallback-path test. | Phase 1 clock HAL |
| Watchdog/fault record | `hal_wdt` owns WWDT0 with an approximate 1-second watchdog period and main-loop servicing. The 32-byte, format-2 `lib_crash` record stores reset cause, reason plus active exception number, stacked PC/xPSR, sequence, and CRC in `.noinit`; it is host-tested and safely rejects invalid frame locations. The WWDT reset cause and a deliberate HardFault record are bench-proven. | Power-cycle retention and watchdog normal-operation timing remain pending. |
| Image identity | A format-1 64-byte identity is fixed at the last 64 bytes of MAIN flash. The host stamps the canonical ELF and verifies matching BIN/HEX artifacts with CRC-32 over defined flash bytes plus the zeroed-CRC identity block. A target read-back match is bench-proven. | Revalidate after linker, artifact, OpenOCD, or board changes; Phase 2 device-info fields must remain format-compatible. |
| LaunchPad UART | The XDS110 backchannel routes target UART0 TX to PB6 (`PINCM17`, function `UART0_TX`) and UART0 RX to PB7 (`PINCM18`, function `UART0_RX`). Phase 1 configures TX only at 115200 baud from BUSCLK with 16x oversampling; RX remains disabled. | EVM target schematic, TI SDK C1106 UART example, and TI device header. The debug boot-banner, a 32-byte lossless burst, and a 128-byte overload/drop-counter test are bench-proven. |
| LaunchPad pins | Red LED has been verified from the target schematic and bench: PA0 blinks on the reference board. I2C pin assignments remain **pending**. | Phase 1 board/HAL |
| Open debug/flash tool | The MAIN-only OpenOCD/XDS110 configuration was validated with source commit `cb52502e88832386610add9781030f5380344063`. Its MSPM0 driver displays this C1106 as generic `mspm0x` because DeviceID `0xbbba` is not in that source's name table, but factory registers correctly reported the expected 64 KiB MAIN/8 KiB SRAM geometry and programming verified successfully. | Revalidate if the configuration, OpenOCD version, or board changes. |
| BSL | **pending:** invocation, configuration, password, and recovery behaviour. | Phase 2 BSL feasibility spike |
| MSPM33C321 | **pending:** no constants or headers are committed until its datasheet, TRM, errata, SDK licence, and reference-board path are recorded. | Phase 5 entry |
