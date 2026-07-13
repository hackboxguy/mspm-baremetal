# Firmware image identity

Every final firmware image reserves the last 64 bytes of MAIN flash for a
host-generated identity header. On MSPM0C1106 the block is
`0x0000FFC0`–`0x0000FFFF`; the linker forbids ordinary loadable content from
overlapping it.

`tools/stamp_image_identity.py` patches the ELF immediately after linking.
That patched ELF is canonical: the BIN and HEX are derived from it, OpenOCD
programs it, and `make identity-check` compares all three artifacts. The tool
uses only Python's standard library and validates the ELF32 load segments,
linker symbols, Intel HEX checksums, and the serialized erased gap.

## Format version 1

All multibyte fields are little-endian. The `VERSION` command-line value is
stored as two BCD bytes; for example, `VERSION=01.02` encodes `0x01, 0x02`.

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| `0x00` | 4 | magic | ASCII `MSPM` |
| `0x04` | 2 | format | Identity format version, currently `1` |
| `0x06` | 2 | header size | `64` |
| `0x08` | 1 | version major | BCD major firmware version |
| `0x09` | 1 | version minor | BCD minor firmware version |
| `0x0A` | 2 | flags | Bit 0: source tree dirty; bit 1: debug build; all other bits zero |
| `0x0C` | 4 | image span | Bytes from MAIN-flash origin through the identity end (`0x10000` for C1106) |
| `0x10` | 4 | CRC-32 | CRC-32/ISO-HDLC; this field is zero during calculation |
| `0x14` | 16 | source ID | ASCII Git revision, normally 12 hex characters, NUL-padded |
| `0x24` | 28 | reserved | Always `0xFF` |

The source ID comes from `git rev-parse --short=12 HEAD`; its dirty bit comes
from `git status --porcelain`. Both can be overridden deliberately with
`SOURCE_ID=` and `SOURCE_DIRTY=` for a release pipeline. A dirty source ID is
still traceable, but is not a claim that the revision alone reproduces the
binary. The final ELF is deliberately relinked and re-stamped for each Make
invocation, so those dynamic fields cannot become stale after a commit or a
working-tree state change. With the same clean sources, toolchain, inputs, and
source ID, its contents remain reproducible; no timestamp is stored.

## CRC coverage and artifacts

The CRC covers exactly two byte ranges, concatenated in address order:

1. `ORIGIN(FLASH)` through (but not including) `__data_load_end`.
2. The entire 64-byte identity header, with bytes `0x10`–`0x13` replaced by
   zero.

The erased gap between `__data_load_end` and `0x0000FFC0` is deliberately
excluded. The BIN still serializes that gap as `0xFF`, while the sparse ELF and
HEX do not manufacture loadable bytes there. This gives the same identity for
the canonical ELF, the derivative BIN/HEX files, and a target programmed from
the ELF.

Build and validate locally:

```sh
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off VERSION=01.02
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off VERSION=01.02 identity-check
```

After flashing that exact ELF, compare the target's read-only 64-byte flash
read-back with it:

```sh
make OPENOCD=/path/to/openocd BOARD=lp_mspm0c1106 APP=blink \
  DEBUG=off VERSION=01.02 identity-readback
```

The read-back command only halts and resumes the target and reads MAIN flash;
it does not erase or program any region.
