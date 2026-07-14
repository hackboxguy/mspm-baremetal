# I2C register-map contract

**Status:** Phase 2 target slice. `lib_regmap` and the register-free target
transaction engine are host-tested. The C1106 target at I2C1 address `0x42`
has also passed initial 100 kHz Raspberry Pi I2C_RDWR pointer-write,
repeated-start-read, and current-address-read checks. Negative/recovery
characterization and the documented `i2ctransfer` fixture remain pending, so
this is not yet a supported field bus.

## Wire protocol

- The target uses a board-defined 7-bit I2C address.
- The register pointer is an unsigned 16-bit big-endian address. It powers on
  as `0x0000`, increments after every data byte, wraps from `0xFFFF` to
  `0x0000`, and persists across STOP conditions.
- Reads from an unmapped address return `0xFF`. Writes to an unmapped or
  read-only address are ignored; they still advance the pointer.

```text
Write: [START] [ADDR+W] [addr_hi] [addr_lo] [data0] ... [STOP]
Read:  [START] [ADDR+W] [addr_hi] [addr_lo] [RESTART] [ADDR+R] [data0] ... [NACK] [STOP]
Current-address read: [START] [ADDR+R] [data0] ... [NACK] [STOP]
```

The pointer changes only after both address bytes arrive. A STOP, target error,
or repeated START before the second byte leaves the previous pointer unchanged.
A completed two-byte address phase commits the new pointer even if
it is immediately followed by a repeated-start read and has no write data.

## Read snapshots and multi-byte values

On every `ADDR+R`, the target calls `lib_regmap_begin_read()`. It first
releases any previous latch, so a reader leaked by an aborted transaction can
block a publish only until the next address-read transaction begins. The first
byte read from a registered page acquires that page's published snapshot. The
same snapshot remains in use until the read moves to another page or the
target calls `lib_regmap_end_read()` for STOP (which the initial target requires
after a final NACK), error recovery, or a new address phase. A publisher uses
the inactive buffer and returns failure rather
than overwriting a snapshot held by an active read; main-loop code retries
later and records the rejection in the snapshot's publish-rejected counter.

Consequently, a multi-byte value is coherent when all of its bytes are read in
one transaction from one page. Multi-byte values are big-endian and masters
must read high byte then low byte in that single transaction. There is no
cross-transaction latch: a current-address read starting at a low byte gets a
new transaction snapshot. Pages must keep any value requiring atomic
presentation within one page.

## Write execution model

The I2C target ISR never runs an application command directly. For a read-write
page, `lib_regmap_write_current()` sends each address/value pair to that page's
queue callback. The standard `lib_regmap_command_queue_t` is SPSC: I2C is the
producer and the main loop is the consumer. It preserves order. A full queue
rejects the write and increments its observable dropped-command counter; the
wire pointer still advances. Page owners publish a new read snapshot only after
the main loop has applied the queued effect.

This makes reads bounded in the ISR and assigns command lifetime explicitly:
a command begins at receipt, is pending while queued, and is complete only
when the main loop applies it. Future status/debug pages must expose pending,
error, dropped-command, and publish-rejected information before they offer
asynchronous commands.

## Standard address allocation

| Address range | Page | Access | Phase 2 status |
|---|---|---|---|
| `0x0000-0x00FF` | Device information | RO | first eight bytes defined below; rest reserved |
| `0x0100-0x01FF` | Status | RO | reserved for application state |
| `0x0200-0x02FF` | Control | RW | queued main-loop commands only |
| `0x0300-0x03FF` | Debug | RW/RO | reserved pending standard fields |
| `0x0400-0x0417` | Crash record | RO | canonical `lib_crash` image defined below; a later target demo publishes it |
| `0x0418-0x0FFF` | Standard expansion | reserved | unimplemented |
| `0x1000-0x1FFF` | Diagnostics | RO | reserved; preserves the RH850 live diagnostics allocation |
| `0x2000-0xEFFF` | Application space | board/application defined | unimplemented |
| `0xF000-0xFEFF` | Future update space | reserved | explicitly not a staging implementation |
| `0xFF00-0xFFFF` | Future boot space | reserved | explicitly not a bootloader implementation |

### Device information (`0x0000-0x0007`)

| Address | Field | Encoding |
|---:|---|---|
| `0x0000` | firmware major | BCD |
| `0x0001` | firmware minor | BCD |
| `0x0002` | build year high | BCD |
| `0x0003` | build year low | BCD |
| `0x0004` | build month | BCD |
| `0x0005` | build day | BCD |
| `0x0006` | build hour | BCD |
| `0x0007` | build minute | BCD |

`lib_buildinfo` validates the BCD representation and writes these eight bytes.
The all-zero date/time is the deliberate reproducible-build value: it means no
timestamp was supplied by the release pipeline. The platform never uses
`__DATE__` or `__TIME__`, so identical inputs do not acquire an accidental
timestamp.

### `i2c_regmap_demo` target status (`0x0300-0x0307`)

The initial C1106 target demo uses this read-only debug subset. It is a demo
diagnostic page, not a product command interface.

| Address | Field | Encoding |
|---:|---|---|
| `0x0300-0x0303` | target transport error count | unsigned 32-bit big-endian |
| `0x0304-0x0307` | status-page publish-rejected count | unsigned 32-bit big-endian |

The error count covers target timeout, FIFO underflow/overflow,
arbitration-loss, interrupt-overflow, and impossible software/hardware state
events. It does not make a bus recovery claim: after an error the target drops
its software transaction state and waits for the controller to terminate with
STOP before a new transaction.

### Crash record (`0x0400-0x0417`)

`lib_crash_write_register_image()` converts the CRC-validated retained record
to this 24-byte big-endian image. A register-map application publishes the
whole image as one read-only page; the target ISR only reads the page snapshot.
It never reads, clears, acknowledges, or otherwise changes retained fault
state. An invalid or unavailable retained record is an all-zero image.

| Offset | Field | Encoding |
|---:|---|---|
| `0x00` | valid | `0x01` for a validated record; `0x00` otherwise |
| `0x01` | reason | `lib_crash_reason_t` code; zero when invalid |
| `0x02-0x03` | record format | unsigned 16-bit big-endian; zero when invalid |
| `0x04-0x07` | sequence | unsigned 32-bit big-endian |
| `0x08-0x0B` | reset cause | raw reset-cause word, unsigned 32-bit big-endian |
| `0x0C-0x0F` | active exception | unsigned 32-bit big-endian |
| `0x10-0x13` | stacked PC | unsigned 32-bit big-endian |
| `0x14-0x17` | stacked xPSR | unsigned 32-bit big-endian |

There is deliberately no acknowledge or clear register in this first
diagnostics contract. The owner and reset semantics of a future acknowledge
operation must be defined before any writable crash-page field is added.

## Cross-platform allocation and RH850 comparison

The address width, byte order, pointer persistence, auto-increment, unmapped
read value, and read-only write behavior match the existing RH850 register-map
document. This contract resolves behavior that its current implementation does
not yet guarantee: short address phases do not change the pointer, every
transaction uses a page snapshot, and writes are queued rather than executed
in the I2C ISR.

The following shared allocations are kept in both protocol documents:

| Address range | MSPM0 status | RH850 status |
|---|---|---|
| `0x0000-0x03FF` | common device-info, status, control, and debug pages | same page allocation; field ownership remains application-specific |
| `0x0400-0x0417` | canonical read-only crash-record image | reserved for that image; current firmware does not implement it |
| `0x1000-0x1FFF` | diagnostics reserved in Phase 2 | diagnostics; `0x1000-0x1003` carry live backlight measurements |

The RH850 firmware has not changed as part of this MSPM0 work. Until a
coordinated RH850 firmware update adopts the snapshot and queued-command
semantics, the two implementations must not be claimed protocol-equivalent for
snapshot or command timing.

## Deferred hardware gate

The C1106 target source implements the transaction boundaries above:
`START` begins a receive or read snapshot; a repeated `START` discards a short
address phase and begins the next direction; `STOP` and error paths release the
read snapshot. It uses automatic receive ACKs and waits two module clocks after
`SRXDONE` before reading RX data (I2C_ERR_08); target-transmit bytes are paced
by the hardware `TREQ` state. It never toggles target `ACTIVE` after
initialization (I2C_ERR_05), has no low-power target wakeup (`SWUEN` is
disabled for I2C_ERR_04), and requires controller transfers at 100 kHz or
above with a terminating STOP (I2C_ERR_09/10).

`hal_i2c_controller` remains deferred. Before the target can be called
supported, the documented external-master fixture must verify address
write/read/repeated-start/current-address behavior, short/aborted
transactions, error recovery, target reset mid-transfer, stuck-SDA recovery,
SCL-low timeout behavior, pull-up value/location, bus voltage, controller
tool version, and the declared 100 kHz bus speed.
