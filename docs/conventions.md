# Conventions

## Language and module boundaries

- Firmware is C11 with fixed-width `<stdint.h>` types and builds warning-free
  with `-Wall -Wextra -Werror -Wconversion -Wshadow -Wundef`.
- Applications include public board, HAL, and library headers only.  They do
  not include `ti/` headers or access registers.
- `lib/` is register-free and host-testable.  Peripheral instances belong in
  HAL configuration/handles, never copied board-specific drivers.
- Dynamic allocation and `printf` are forbidden.  The linker script exposes a
  zero-sized heap; future debug output goes through `lib_debug` and UART TX.

## Startup and memory

- `startup.c` and `linker.ld` are owned code.  `.data` is copied, `.bss` is
  zeroed, and `.noinit` is not touched unless the verified reset/ECC policy
  deliberately invalidates it.
- The raw reset cause is captured before documented clear semantics and is
  retained for later diagnostics.
- The fixed flash image-identity block is reserved now.  Phase 1 patches the
  ELF first, then derives matching HEX and BIN artifacts.

## Interrupts and concurrency

- Cortex-M0+ has four priority levels: 0 is highest and 3 is lowest.  The
  Phase 1 blink image enables only SysTick, at priority 3.  No other exception
  or peripheral interrupt may pre-empt it yet, and no SPSC pair exists in this
  image.  Before enabling a further interrupt, document its priority, nesting
  relationship, and every producer/consumer pair here.
- ISR work is bounded.  Register-map page execution context is decided before
  the Phase 2 `lib_regmap` API freezes; command work normally belongs in the
  main loop.

## Device safety

- MAIN flash is the only region normal development commands may program.
  BCR/NONMAIN/BSL configuration needs a dedicated production-security plan.
- Every hardware constant in owned code must trace to `docs/device_facts.md`
  and its primary source or bench record.
