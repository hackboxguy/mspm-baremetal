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

## Remaining Phase 1 evidence

- GDB client attach and known-symbol stop through `make ... gdb`.
- Clock-frequency measurement and fallback-path test.
- UART pin assignment, boot banner, and loss/overflow behaviour.
- Watchdog, fault record, reset-cause, and SRAM parity/ECC evidence.
- Canonical image-identity patching plus artifact/read-back comparison.
