# Device facts

**Status:** Phase 1 bring-up in progress. Entries marked **pending** still need
the applicable TRM, errata, and bench evidence before their dependent work is
accepted.

## Sources

| Source | Revision / date | Use |
|---|---|---|
| [MSPM0C1105/MSPM0C1106 datasheet](https://www.ti.com/lit/ds/symlink/mspm0c1106.pdf) | Rev. B, January 2026 | memory map, core frequency, device capabilities |
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
| Reset/run clock policy | MCLK resets to SYSOSC; Phase 1 explicitly selects SYSOSC with no divider and requests its 32 MHz base frequency.  If bounded status polling fails, it requests the 4 MHz SYSOSC fallback. | TI SDK `dl_sysctl_mspm0c1105_c1106.h` and device register header; this is encoded in `hal_clock`. |
| Header provenance | TI BSD-3-Clause device headers and Apache-2.0 CMSIS subset | `ti/README.md`; the vendored source is pinned above. |

## Decisions and pending evidence

| Topic | Current rule | Required before |
|---|---|---|
| Flashing | No `flash` target exists yet.  Normal workflows must program MAIN flash only and exclude BCR/BSL configuration. | Phase 1 flash/debug command |
| SRAM parity/ECC | **pending:** determine whether SRAM initialisation is required and how it interacts with `.noinit`. | Phase 1 startup/fault record |
| Reset cause | **pending:** document SYSCTL reset-cause bits, read/clear semantics, and retention.  Startup will preserve the raw value before any clear operation. | Phase 1 startup/fault record |
| Run clock | SYSOSC 32 MHz configuration and bounded 4 MHz fallback are implemented. **Pending:** bench timing and the full TRM/errata review before the clock service is accepted. | Phase 1 clock HAL |
| LaunchPad pins | Red LED has been verified from the target schematic. **Pending:** bench confirmation, UART, SWD, and I2C pin assignments. | Phase 1 board/HAL |
| Open debug/flash tool | A MAIN-only OpenOCD/XDS110 probe configuration is prepared in `tools/openocd/`.  The upstream MSPM0 driver derives flash geometry from factory registers, so its missing C1106 display-name entry is not a programming limitation. **Pending:** read-only probe and MAIN-only program/readback on this board. | Phase 1 tool integration |
| BSL | **pending:** invocation, configuration, password, and recovery behaviour. | Phase 2 BSL feasibility spike |
| MSPM33C321 | **pending:** no constants or headers are committed until its datasheet, TRM, errata, SDK licence, and reference-board path are recorded. | Phase 5 entry |
