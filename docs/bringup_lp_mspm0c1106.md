# LP-MSPM0C1106 bring-up record

## 2026-07-13 — MAIN-only blink flash

| Item | Record |
|---|---|
| Board | LP-MSPM0C1106; board revision was not recorded. |
| Host | WSL2; distribution/version was not recorded. |
| Debug probe | Onboard XDS110, firmware `3.0.0.36`, hardware `0x0028`. |
| OpenOCD | `0.12.0+dev-gcb52502`, built from upstream commit `cb52502e88832386610add9781030f5380344063` with the XDS110 and MSPM0 drivers. |
| Firmware | Release `blink`, `VERSION=01.02`, source commit `ae38d56`; image identity remains the Phase 1 placeholder. |
| USB note | WSL2 initially exposed the XDS110 USB node as `root:root`; the temporary `sudo chmod a+rw /dev/bus/usb/001/002` workaround was used for this session. |

Build command:

```sh
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off VERSION=01.02
```

Read-only probe command:

```sh
OPENOCD=/tmp/openocd-source/src/openocd tools/openocd/probe_lp_mspm0c1106.sh
```

The MSPM0 driver displayed DeviceID `0xbbba` as generic `mspm0x`, but its
factory-register probe reported 64 KiB MAIN flash in one bank, 8 KiB SRAM, and
no data flash. This matches the device facts used by the linker.

Program/verify/reset command:

```sh
/tmp/openocd-source/src/openocd -f tools/openocd/lp_mspm0c1106.cfg \
  -c 'program output/lp_mspm0c1106/blink/release/lp_mspm0c1106_blink.elf verify reset exit'
```

Observed result: OpenOCD completed programming and returned `** Verified OK **`.
A post-reset SWD halt found the PC at `0x00000100`, which maps to `main()` in
`app/blink/main.c`. After resuming the target, the operator observed the
active-high red LED on PA0 blinking at the expected visible rate.

The supported Make wrapper was then run against the same board and returned
`** Verified OK **`:

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=blink DEBUG=off VERSION=01.02 flash
```

The configuration used for both commands declares MAIN flash only. It has no
NONMAIN, BCR, BSL, DATA-flash, mass-erase, or factory-reset command.

## 2026-07-13 — UART0 TX and debug transport

The XDS110's application/user UART ACM interface was exposed as `/dev/ttyACM0`
in WSL2. The terminal was configured before the target was reset:

```sh
stty -F /dev/ttyACM0 115200 cs8 -cstopb -parenb -ixon -ixoff raw -echo
cat /dev/ttyACM0
```

The uncommitted debug `blink` build was then programmed through the same
MAIN-only OpenOCD configuration:

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02 flash
```

OpenOCD returned `** Verified OK **` and the terminal captured
`mspm-baremetal: blink`. This validates UART0 TX on PB6 at 115200 baud through
the XDS110 backchannel. RX remains deliberately unconfigured. The banner is
issued through `lib_debug`, whose writer is installed only in debug builds;
the release ELF has no `lib_debug` or debug-writer symbol after link-time
garbage collection.

## 2026-07-13 — WWDT reset and retained reset cause

The updated debug `blink` image starts WWDT0 only after the timer and UART
bring-up succeeds. It uses the default 32.768 kHz LFCLK, a `2^15` count, and
no closed servicing window (nominally one second); the main loop services it
before every WFI.

For a controlled expiry test, OpenOCD temporarily changed the watchdog's
debug behavior to free-run while the core was halted, then read the retained
record after reset:

```sh
/tmp/openocd-source/src/openocd -f tools/openocd/lp_mspm0c1106.cfg \
  -c 'init' -c 'halt' -c 'mww 0x40081018 0x1' -c 'sleep 1500' \
  -c 'halt' -c 'mdw 0x2000007c 8' -c 'shutdown'
```

The first halted-core sequence reset the device. A follow-up reset-halted read
reported `43525348 00200001 00000000 0000000e 00000000 00000000 00000000
8af9cb6f`: the crash-record magic, format/size, zero sequence, WWDT0 reset
cause (`0x0E`), no exception reason, and CRC. The normal debug configuration
stops the watchdog under SWD so interactive debugging does not cause a reset.

## 2026-07-13 — deliberate HardFault record

`fault_record_test` is a dedicated manual-test application. It first triggers
one `udf #0` instruction, which is a HardFault on Cortex-M0+, then recognizes
the valid retained record after reset and remains healthy instead of faulting
again.

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=fault_record_test DEBUG=on VERSION=01.02 flash
```

OpenOCD returned `** Verified OK **`; the UART captured the retained-fault
banner. A reset-halted record read at `0x2000007C` returned
`43525348 00200001 00000001 0000001d 00000002 00000140 01000000 34a71d15`:
valid magic and CRC, sequence 1, software CPU-reset cause `0x1D`, HardFault
reason 2, the stacked `udf` PC `0x00000140`, and xPSR `0x01000000`.

## Troubleshooting

- Before the Phase 1 fault recorder is implemented, every unhandled exception
  enters `Default_Handler`, a WFI loop. A frozen LED can therefore be a fault,
  not a healthy idle loop. Attach with SWD, halt the target, and inspect the PC
  before assuming a timer or GPIO problem.

## Remaining Phase 1 evidence

- GDB client attach and known-symbol stop through `make ... gdb`.
- Clock-frequency measurement and fallback-path test.
- UART TX burst and overflow/drop-counter evidence. The boot banner and
  terminal path are verified on `/dev/ttyACM0` at 115200 baud.
- Power-cycle `.noinit` retention and watchdog normal-operation timing.
- Canonical image-identity patching plus artifact/read-back comparison.
