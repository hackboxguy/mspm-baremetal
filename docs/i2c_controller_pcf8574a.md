# I2C controller / PCF8574-compatible acceptance

`pcf8574a_demo` is the first hardware-facing I2C-controller diagnostic for
the LP-MSPM0C1106. It exercises the polled I2C1 controller backend, including
the hardware `RD_ON_TXEMPTY` combined-transfer path: write, repeated START,
and read in one transaction.

## Fixture

Disconnect the Raspberry Pi and every other I2C controller before connecting
this fixture. Connect one PCF8574A or PCF8574-compatible module as follows:

| PCF8574A module | LP-MSPM0C1106 |
|---|---|
| VCC | 3.3 V only |
| GND | GND |
| SCL | PB2, BoosterPack position 9 |
| SDA | PB3, BoosterPack position 10 |
| INT | leave unconnected |

The LaunchPad I2C pull-up footprints are DNC. Confirm the module provides a
single suitable 3.3 V pull-up pair, or fit one external pair. Do not let a
5 V-powered module pull SCL or SDA high. The PCF8574A address pins select one
address from `0x38` through `0x3F`. If no A-suffix address responds, the app
also safely checks `0x20` through `0x27` for a compatible non-A PCF8574
substitution; every test write remains `0xFF`.

## Run

Build and program the debug image with the validated MAIN-only OpenOCD path:

```sh
make OPENOCD=/tmp/openocd-source/src/openocd \
  BOARD=lp_mspm0c1106 APP=pcf8574a_demo DEBUG=on flash
```

The app first writes `0xFF` to a responding address. On PCF8574-compatible
devices this releases all eight quasi-bidirectional ports, so the probe does
not intentionally drive an output low. It next performs a separate one-byte
read, then repeats the `0xFF` write and reads one byte through a repeated-START
combined transaction.

Success is a 500 ms red-LED blink and the backchannel text
`PCF8574A_COMBINED_OK`. A steady red LED is a failure. In that case halt with
SWD and inspect `g_pcf8574a_stage`, `g_pcf8574a_result`,
`g_pcf8574a_address`, and `g_pcf8574a_port_value`; the source enums give the
exact stage and result code.

## Recorded acceptance

On 2026-07-14, the attached 3.3 V module acknowledged the safe write at
`0x20`, not in the PCF8574A `0x38`–`0x3F` range. The standalone read and the
combined write/repeated-START/read both completed successfully and returned
`0xFF`; the app reached `PCF8574A_DEMO_STAGE_COMPLETE` with result `0`. This
is positive controller-path evidence for a PCF8574-compatible fixture. The
address response identifies the connected module or its strapping as the
standard PCF8574 address family, regardless of its board label.

The current backend limits each write and read portion to four bytes. It
enables the peripheral SCL-low timeout and has a separate bounded polling
ceiling. It reports a low SDA line before START rather than clocking GPIO
recovery pulses; correct the fixture power/wiring or remove the target before
retrying. This is deliberately not a claim of recovery: a board-owned
GPIO-remux 9-clock recovery helper and its HIL test remain required for the
Phase 2 stuck-bus gate.
