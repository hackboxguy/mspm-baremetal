# Conventions

## Language and module boundaries

- Firmware is C11 with fixed-width `<stdint.h>` types and builds warning-free
  with `-Wall -Wextra -Werror -Wconversion -Wshadow -Wundef`.
- Applications include public board, HAL, and library headers, plus narrow
  architecture abstractions such as `arch_wait.h`. They do not include `ti/`
  headers or access registers.
- `lib/` is register-free and host-testable.  Peripheral instances belong in
  HAL configuration/handles, never copied board-specific drivers.
- Dynamic allocation and `printf` are forbidden.  The linker script exposes a
  zero-sized heap; future debug output goes through `lib_debug` and UART TX.
- Update `CLAUDE.md` in the same commit as any implemented phase-gate or
  supported-workflow change. It is an operational status document, not a
  historical proposal.

## Startup and memory

- `startup.c` and `linker.ld` are owned code.  `.data` is copied, `.bss` is
  zeroed, and `.noinit` is not touched unless the verified reset/ECC policy
  deliberately invalidates it.
- `Reset_Handler` reads the read-to-clear raw reset cause before normal C
  initialization and commits it to the CRC-protected `.noinit` crash record.
  The record is never trusted until its magic, format, size, and CRC validate.
  Fault capture commits the magic word last, then requests a system reset.
- The fixed flash image-identity block is host-stamped in the canonical ELF
  before the HEX and BIN are derived. Its CRC covers defined flash content from
  `ORIGIN(FLASH)` through `__data_load_end`, followed by the identity block
  with its CRC field zeroed. It excludes the erased gap between those ranges.
  The BIN serialises that gap as `0xFF`, matching erased MAIN flash; an
  ELF-programmed target does not write the gap. The exact format and
  read-back procedure are in [image_identity.md](image_identity.md).
- `lib_crash` records a packed reason code and the active exception number.
  Its format is CRC-gated. `lib_crash_write_register_image()` safely encodes a
  validated record for a read-only snapshot page; it has no public
  clear/acknowledge operation. Phase 2 must define owner and reset semantics
  before adding any writable crash-page field.

## Interrupts and concurrency

- Cortex-M0+ has four priority levels: 0 is highest and 3 is lowest. The
  Phase 2 I2C register-map demo uses I2C1 at priority 1, UART0 at priority 2,
  and SysTick at priority 3. I2C1 can pre-empt UART0 and SysTick; UART0 can
  pre-empt SysTick. The UART0 TX SPSC producer is the main thread and its
  consumer is the priority-2 UART0 ISR.
  The UART TX queue has no ISR producer: a future producer must not run above
  priority 2 or share this queue without a new concurrency design. UART IMASK
  updates use a short PRIMASK critical section so future RX/error mask bits
  cannot be lost to a thread/ISR read-modify-write race.
  The `i2c_regmap_demo` build assigns I2C1 to the target transaction engine at
  priority 1. It is the
  `lib_regmap` snapshot reader and queued-command producer; the main loop is
  the snapshot publisher and queued-command consumer. It neither executes a
  command callback nor changes a page snapshot in the ISR. Before enabling a
  further interrupt, document its priority, nesting relationship, and every
  producer/consumer pair here.
- `pcf8574a_demo` instead owns I2C1 as a polling controller: it enables no
  I2C1 interrupt and never starts the target backend. An application selects
  exactly one I2C1 personality; target and controller use of the same instance
  are not concurrent. The board wrapper enforces this once-only claim at
  runtime and returns `false` if either personality was already initialized.
- `lib_ringbuf` is a fixed-storage, byte-oriented SPSC primitive. Its capacity
  is a non-zero power of two; it uses every slot and monotonically advancing
  32-bit masked indices. The producer exclusively calls `try_push` and owns
  overflow accounting; the consumer exclusively calls `try_pop`. Its compiler
  fences are sufficient only for one Cortex-M core and its interrupts, not DMA
  or multi-core sharing. A diagnostic may read a stale dropped-count snapshot.
- `lib_regmap` uses a 16-bit EEPROM-style pointer. Its page snapshots are
  acquired on the first byte of each I2C read transaction and released on the
  transaction boundary; the producer publishes a new snapshot only when its
  inactive buffer has no active reader. A new address-read transaction releases
  any stale snapshot latch left by an aborted read; rejected publishes increment
  a diagnostic counter. I2C-to-main command delivery is an
  ordered SPSC queue: the I2C ISR is its producer and the main loop is its
  consumer. No register-map callback runs application command work in an ISR.
  The full wire and snapshot contract is in
  [i2c_register_map.md](i2c_register_map.md).
- `lib_debug` is the only application/driver debug-output path. It is
  register-free and delegates to a board-installed byte-writer callback. Use
  `DBG_WRITE_LITERAL` or `DBG_WRITE_BYTES`; both compile to no runtime work in
  release builds. The LP-MSPM0C1106 board installs its UART0 TX writer only in
  debug builds.
- WWDT0 is owned by `hal_wdt`. It has a 1-second LFCLK period, no closed
  servicing window, runs while the core uses WFI, and stops while SWD halts the
  core. The main-loop health path—not an ISR—services it.
- Phase 1 uses a deliberate bench policy: failures before watchdog start hold
  a distinct LED state for inspection; test-app assertion failures may keep
  kicking the watchdog for the same reason; after watchdog start a normal app
  that stops kicking resets. Phase 3 product apps must start the watchdog once
  minimal safety initialization completes, never kick it in a persistent
  failure state, and use the crash-record sequence counter to cap reset-storm
  retries before entering a defined safe state.
- ISR work is bounded.  Register-map page execution context is decided before
  the Phase 2 `lib_regmap` API freezes; command work normally belongs in the
  main loop.

## Device safety

- MAIN flash is the only region normal development commands may program.
  BCR/NONMAIN and any future field-update bootloader need a dedicated
  production-security plan. MSPM0C1106 has no ROM-BSL configuration.
- Every hardware constant in owned code must trace to `docs/device_facts.md`
  and its primary source or bench record.
- Unhandled exceptions enter the CRC-gated fault recorder, which captures the
  active exception number and requests a reset. A frozen LED can therefore
  indicate an early initialization failure rather than a healthy idle loop;
  use the documented SWD procedure to distinguish them.
