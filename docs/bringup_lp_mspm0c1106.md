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
again. In a release build the retained-fault state has a distinct 125 ms red
LED toggle cadence; debug additionally prints its retained-fault banner.

The test intentionally does not trigger again while a valid retained record
exists. Power-cycle before a repeat attempt, but do not assume that this clears
`.noinit`: power-cycle retention is still an open Phase 1 check. If the record
survives, the app will remain in the retained-fault state rather than erase
diagnostic evidence; repeat testing then needs the future Phase 2 acknowledge
semantics or an explicitly controlled debugger SRAM clear.

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=fault_record_test DEBUG=on VERSION=01.02 flash
```

OpenOCD returned `** Verified OK **`; the UART captured the retained-fault
banner. A reset-halted record read at `0x2000007C` returned
`43525348 00200001 00000001 0000001d 00000002 00000140 01000000 34a71d15`:
valid magic and CRC, sequence 1, software CPU-reset cause `0x1D`, HardFault
reason 2, the stacked `udf` PC `0x00000140`, and xPSR `0x01000000`.

## 2026-07-14 — hardened format-2 HardFault capture

The format-2 fault path validates the hardware stack-frame address in its naked
handler and snapshots PC/xPSR before switching to a fresh MSP for C code. This
matters when a normal near-top-of-SRAM exception frame would otherwise be
overwritten by nested C calls on that fresh stack; it also leaves invalid frames
as the documented zero-PC/xPSR sentinel.

The updated debug `fault_record_test` was programmed and its retained record
was read with the core reset-halted after its one deliberate `udf #0`:

```text
43525348 00200002 00000001 0000001d 00000302 00000174 01000000 f4dc8fbb
```

The record has format/size `0x00200002`, sequence 1, CPU-software reset cause
`0x1D`, and a packed HardFault reason 2 plus active IPSR 3 (`0x00000302`). Its
PC points to the test `udf` (`0x00000174`), its xPSR is `0x01000000`, and its
CRC validates. This closes the format-2 fault-frame capture gate.

## 2026-07-13 — canonical image identity

The post-link identity tool now patches a fixed 64-byte block at
`0x0000FFC0` in the canonical ELF, then derives the BIN and HEX artifacts.
For the release `blink` build (`VERSION=01.02`) tested here, the identity was
format 1, source ID `ac2a5153c91c`, flags `0x0001` (dirty working tree), span
`0x10000`, and CRC32 `0x5ebe198c`. `make ... identity-check` verified that the
ELF, BIN, and HEX contained exactly the same identity and defined flash
content; the BIN's intervening gap was verified as `0xFF`.

The same canonical ELF was programmed with the validated MAIN-only
program-and-verify command. A separate command then halted the target, read
only `0x0000FFC0`–`0x0000FFFF`, compared the 64 bytes to the ELF, resumed the
target, and reported a match:

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=blink DEBUG=off VERSION=01.02 identity-readback
```

This closes the Phase 1 image-identity artifact and target read-back gate.

## 2026-07-14 — clean Phase 2 foundation image

After commit `2b72ed2` added the portable register-map foundation, the
LP-MSPM0C1106 was reattached and first passed the read-only SWD/factory-geometry
probe. The freshly built clean release `blink` image was then programmed through
the same MAIN-only configuration and returned `** Verified OK **`.

The follow-up identity readback used the normal command:

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=blink DEBUG=off VERSION=01.02 identity-readback
```

It matched the canonical ELF exactly: version `01.02`, source ID
`2b72ed278a62`, clean flags `0x0000`, content length `0xC04`, and CRC32
`0x52B82FF5`. This test made no BCR, BSL, NONMAIN, or DATA-flash change.

## Phase 2 fixture preparation — I2C and bootloader decision

The first I2C target will use I2C1 on the board's exposed BoosterPack-standard
I2C pair:

| Signal | LP-MSPM0C1106 pin | IOMUX | BoosterPack position |
|---|---|---|---|
| SCL | PB2 | `PINCM11`, `I2C1_SCL` | 9 |
| SDA | PB3 | `PINCM12`, `I2C1_SDA` | 10 |

The EVM schematic marks the I2C pull-up footprints R3 and R11 DNC. Before
connecting a Raspberry Pi or another controller, use one external 3.3 V
pull-up pair and a common ground. Do not put 5 V on either I2C signal or on the
3.3 V rail. The fixture record must name the controller, operating system,
`i2c-tools` version, pull-up value/location, measured or justified bus speed,
and exact commands before target acceptance is claimed.

The C1106 has no ROM BSL: a UART/I2C field-update path would instead be a
user-owned flash BSL at reset address `0x0`. The present `blink` image has no
such firmware, so there is no safe generic BSL probe to run. J101 exposes the
XDS110 reset/BSL-invoke signals and J3/S1 drives PA18, but those signals only
matter after a deliberately designed flash BSL implements an invocation policy.

### I2C target source slice (not hardware acceptance)

`app/i2c_regmap_demo` now builds a C1106 I2C1 target at 7-bit address `0x42`.
It exposes device information at `0x0000`, target diagnostics at `0x0300`, and
the read-only crash image at `0x0400`. The source uses open-drain PB2/PB3,
target clock stretching, manual receive ACK, and an initial 100 kHz controller
fixture. It disables target low-power wake (`SWUEN`) and never changes target
`ACTIVE` after initialization in accordance with the C1106 I2C errata.

This is a source/build milestone only. No I2C target image has been programmed
and no external controller has been connected or tested. The first hardware
session must use the 3.3 V external pull-up pair described above, send STOP at
the end of every transfer, and record the fixture before treating the interface
as supported.

Before UniFlash was installed, this WSL host had no TI UniFlash/DSS executable
(`dslite` or `uniflash`). `openjdk-17-jre` is only a runtime prerequisite; it
does not install the UniFlash programming support needed for the separate
MSPM33 workflow. No BSL command has been sent, and no BCR, NONMAIN, password,
erase, or configuration change is authorised by this record.

### UniFlash 9.6 tooling result

UniFlash `9.6.0.5764` was subsequently installed at
`/home/testpc/ti/uniflash_9.6.0`. Its target database contains both
`MSPM0C1106` and `MSPM33C321A`. It requires XDS110 firmware `3.0.0.43`; the
attached probe was explicitly updated from `3.0.0.36` through TI's DFU utility,
then reattached to WSL2. The updater identified the probe as serial
`MC010001` before the update and reported `3.0.0.43` afterwards.

The following no-write C1106 probe then returned `Success` and identified core
0 as `CORTEX_M0P`:

```sh
TI_APPDATA_DIR=/tmp/uniflash-appdata \
  /home/testpc/ti/uniflash_9.6.0/dslite.sh \
  --config=/tmp/mspm0c1106_xds110_probe.ccxml --list-cores --verbose
```

The probe invocation supplied no target image, erase, reset, or
device-configuration option. The
separate MSPM33C321A board was subsequently validated the same way: its
XDS110 `M3010001` was updated from `3.0.0.38` to `3.0.0.43`, then its
device-specific `--list-cores` probe identified `CORTEX_M33`, initialized the
memory map, and returned `Success`. These are connection checks only; neither
result is an MSPM33 flash validation.

## 2026-07-13 — UART TX burst and overflow counter

The debug `uart_tx_test` image first enqueues a 32-byte lossless burst, waits
for the TX ISR to drain it, then enqueues one 128-byte overload burst. The
board queue is 64 bytes, so the test accepts exactly its first 64 bytes and
requires `board_uart_backchannel_dropped_count()` to increase before it emits
the final marker. It drives the red LED solid on either assertion failure.

The XDS110 application UART was configured as before at 115200 8N1, then a
15-second capture was started before programming the test image:

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=uart_tx_test DEBUG=on VERSION=01.02 flash
```

The captured byte stream was:

```text
mspm-baremetal: uart_tx_test
0123456789ABCDEF0123456789ABCDEFUART_TX_LOSSLESS_OK
0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFUART_TX_OVERFLOW_OK
```

The first payload is all 32 expected bytes. The second contains four complete
16-byte blocks (64 bytes), no more, followed by `UART_TX_OVERFLOW_OK`; the app
only emits that marker after its overflow counter has advanced. OpenOCD's
MAIN-only binary fallback comparison ended with `** Verified OK **`.

## Troubleshooting

- Unhandled exceptions capture a retained crash record and request a reset. A
  frozen LED can still be an early initialization failure rather than a healthy
  idle loop. Attach with SWD, halt the target, and inspect the PC and
  `g_crash_record` before assuming a timer or GPIO problem.

## Remaining Phase 1 evidence

- GDB client attach and known-symbol stop through `make ... gdb`.
- Clock-frequency measurement and fallback-path test.
- Power-cycle `.noinit` retention and watchdog normal-operation timing.
