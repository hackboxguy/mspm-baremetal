# MSPM0 / MSPM33 Bare-Metal Code Base — Implementation Plan

**Status:** accepted

**Date:** 2026-07-13

**Companion document:** [`mspm-mcu-code-base-structure.md`](mspm-mcu-code-base-structure.md)

## 1. Purpose

This plan turns the architecture proposal into an incremental implementation
programme for a maintainable, terminal-first bare-metal platform for TI MSPM
MCUs.  The first implementation target is MSPM0C1106; MSPM33C321 support is a
second device port within the same repository, not a prerequisite for the
first usable platform.

The intended developer interface remains familiar to the existing RH850
project:

```sh
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off
make BOARD=lp_mspm0c1106 APP=i2c_regmap_demo DEBUG=on VERSION=01.02
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off flash
make DEBUG=off test
```

This document defines implementation order, ownership boundaries, acceptance
criteria, and decisions that must be backed by device documentation or bench
evidence.  It deliberately does not repeat the proposed API or register-level
implementation in full.

## 2. Outcomes and boundaries

### 2.1 Outcomes for the first usable release

The initial MSPM0C1106 platform is complete when it provides all of the
following on a named reference board:

- A reproducible GNU Make build producing ELF, BIN, HEX, map, and dependency
  files from a clean checkout.
- Owned startup and linker code that correctly initializes `.data`, `.bss`,
  `.noinit`, stack, and vector table.
- GPIO blink, a timer-backed delay/tick service, IRQ-driven UART TX, watchdog
  reset, and a persistent fault record.
- A fixed, read-back-verifiable firmware image-identity block shared by ELF,
  HEX, and BIN artifacts.
- A documented interrupt-priority/nesting policy that covers every ISR and
  SPSC producer/consumer pair.
- I2C controller and target support, including the standard 16-bit,
  EEPROM-style register-map protocol.
- Host tests for all register-independent library code, plus a documented
  on-target bring-up and I2C acceptance procedure.
- CI that builds every supported board/application/variant combination and
  enforces tests, formatting, static-analysis baselines, and size budgets.

### 2.2 Explicit non-goals for the first release

The following are intentionally deferred.  Their interfaces must not be
invented speculatively during the initial bring-up.

- Secure/non-secure TrustZone image split, secure boot, MPU policy, and an A/B
  update bootloader.
- Production flash-protection configuration and field firmware-update flow.
- DMA, CAN-FD/LIN, ADC, CRC, and other peripherals not needed by the first
  board or application.
- Extracting shared code into a third repository or adding a git subtree.
- A hard dependency on emulation, a self-hosted HIL runner, or a particular
  debug probe other than the chosen reference-board setup.
- UART reception: the initial platform is deliberately TX-only; RX is disabled
  until an application owns a defined receive and error-handling policy.

### 2.3 Architectural rules that every phase preserves

1. Applications include public `hal/`, `lib/`, and their selected `board/`
   header only.  They do not include TI device headers or access registers.
2. `lib/` contains no register access and is host-testable without an ARM
   compiler.
3. A board selects its device in `board.mk`; an application does not select an
   MCU or compile a device-specific backend itself.
4. Peripheral drivers are instanced through handles/configuration, never
   copied per peripheral or board.
5. The lowest common denominator is Cortex-M0+: C11, no FPU, four NVIC
   priority levels, and PRIMASK-based critical sections.  M33 enhancements
   remain behind `arch/` and device-specific build code.
6. Startup, linker scripts, and fault handling are owned source with reviewable
   tests and documentation, not opaque SDK copies.
7. The I2C register-map document is a cross-platform protocol contract.  A
   deliberate protocol change requires compatible updates to both projects and
   host tooling.

## 3. Inputs that must be verified before implementation

The architecture proposal correctly labels several items as *verify*.  No
constant, linker memory region, register bit definition, flash procedure, or
tooling claim may be derived from the proposal alone.

Create `docs/device_facts.md` in Phase 0.  For each supported device it shall
record the exact orderable part and package, revision/date of the datasheet and
TRM, SDK/CMSIS package version and license, and a source reference for:

| Topic | Needed before |
|---|---|
| Flash/SRAM sizes, aliases, erase/program units, reserved regions, and ECC/parity initialization, including `.noinit` read safety | linker script and startup |
| MSPM0 NONMAIN/BCR layout, factory defaults, protection controls, tool operations that can modify it, and recovery limits | any flash/debug command or production planning |
| Reset/boot-cause registers, their read/clear semantics, persistence across reset types, BSL invocation, debug-port, and security-state behaviour | startup, diagnostics, flash tools, production planning |
| Clock sources, reset clock, maximum core/peripheral clocks, and UART/I2C clocking rules | clock, UART, I2C |
| Pin mux and electrical capability of reference-board pins | board initialization |
| Interrupt numbers, priority-bit count, peripheral instances, FIFO semantics, and error flags | vector table and HAL IRQs |
| I2C target/controller state-machine requirements and errata | I2C HAL and acceptance tests |
| Tool support and minimum known-good versions for GNU Arm, OpenOCD/pyOCD, and the selected probe | documented developer setup |
| SDK redistribution terms and notices for every vendored item | adding `ti/` content |

The document should distinguish primary sources (datasheet, TRM, errata,
official SDK release notes) from bench observations.  It is a release artifact:
a reviewer must be able to trace every hardware-dependent implementation
constant back to it.

## 4. Delivery sequence

Each phase ends in a reviewable, working increment.  Do not begin a phase that
needs unavailable hardware or unresolved facts; complete independent host and
build work first and record the blocked evidence in `device_facts.md`.

The estimates below are non-binding planning aids, not acceptance criteria:
Phase 0 is small (roughly 1–2 days), Phase 1 is medium (about a week with a
working board), Phase 2 is medium (about 1–2 weeks), and Phases 3–5 depend on
the product hardware and evidence available at their entry gates.

### Phase 0 — foundation and evidence

**Goal:** establish a clean repository that can build a minimal MSPM0 image
without relying on untracked local files.

**Implement**

- Add the proposed top-level directories: `make/`, `ti/`, `device/`, `arch/`,
  `hal/`, `lib/`, `board/`, `app/`, `tests/`, and `tools/`.
- Add licence attribution and an inventory for vendored TI/CMSIS content.  Keep
  only the minimum headers/SVD needed by the build; preserve upstream notices
  and version/source metadata.
- Write `docs/device_facts.md`, a concise `docs/conventions.md`, and the
  project `CLAUDE.md`.  Seed the latter two with the RH850 lessons listed in
  the architecture proposal.  Add the checked-in `clang-format` style before
  implementation code exists.
- Implement the thin top-level `Makefile` and `make/toolchain.mk`,
  `make/rules.mk`, and `make/device_mspm0c1106.mk` fragments.  Require
  `BOARD` and `APP` to resolve to existing directories, validate `VERSION` as
  two BCD byte pairs, default an omitted `VERSION` to `00.00`, select `debug`
  or `release` deterministically, and compare the compiler version with the
  pinned release (warning locally, failure in CI).  The default version is also
  reported as `00.00` by the future image-identity and device-info fields.
- Make the object tree mirror source paths under
  `build/<board>_<app>_<variant>/`; emit artifacts under
  `output/<board>/<app>/<variant>/` as `<board>_<app>.elf`, `.bin`, `.hex`,
  and `.map`.  Generate and include `-MMD -MP` dependencies.
- Add `device/mspm0c1106/linker.ld`, `startup.c`, a minimal
  `board/lp_mspm0c1106/` stub (`board.mk` selects the device and `board.h` is
  intentionally near-empty), and a link-complete `blink` app.  In Phase 0 the
  app is an explicit idle-loop bring-up placeholder, not a claimed physical
  blink; Phase 1 adds GPIO and turns it into the visible smoke test.  The first
  linker script shall define symbols for `.data` load/start/end, `.bss`,
  `.noinit`, a reserved fixed-location image-identity block, a zero-sized heap,
  and stack bounds rather than embedding duplicate literals in C.  Firmware
  does not use `malloc` or `printf`; enforce this with the selected link-time
  policy.
- Select and document the host-test framework (Unity or a small checked-in
  assert harness), require `<stdint.h>` types at library/test interfaces, add
  at least one passing test, and build host tests with `-Wconversion`,
  AddressSanitizer, and UndefinedBehaviorSanitizer.  Exercise a 32-bit host
  build when the runner provides it.
- Add a minimal `.github/workflows/ci.yml`: install the pinned toolchain; build
  `blink` in debug and release; run `make test`; and run
  `clang-format --dry-run`.  Every later phase extends this matrix as it adds
  boards, applications, or host tests.

**Startup contract**

`Reset_Handler` must follow the device-specific sequence recorded in
`device_facts.md`: capture and preserve the raw reset cause for later
application/diagnostic use before any documented read-to-clear or
write-1-to-clear operation; establish the required SRAM/ECC/parity state
without making an uninitialised `.noinit` read fault; copy `.data` from its
load address in flash; zero `.bss`; and preserve or deliberately invalidate
`.noinit` according to that reset cause.  It then calls the C
runtime/application entry point and never returns.  The vector table belongs
in the linker-defined vector region.  All unexpected exceptions route to the
common fault path once it exists; early startup may use a small safe reset
handler until RAM is initialized.

**Acceptance gate**

```sh
make BOARD=lp_mspm0c1106 APP=blink DEBUG=off
make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02
make DEBUG=off test
```

- Both builds produce ELF, BIN, HEX, map, and a source-tree-preserving object
  directory with no warnings.
- Changing a public header and rebuilding recompiles its dependants.
- The map proves `.data` has distinct load and run addresses where applicable,
  `.noinit` is not included in the zeroed range, and flash/RAM regions match
  `device_facts.md`.
- The Phase 0 `blink` target is explicitly a link-complete placeholder; GPIO
  behaviour is not accepted until the Phase 1 board bring-up gate.
- The repository can be built from a clean clone using only documented tool
  installation steps, and the minimal CI workflow proves the same commands on
  every change.

### Phase 1 — MSPM0 reference-board bring-up

**Goal:** prove that startup, clocks, pins, debug, timing, reset recovery, and
the debug connection work on real hardware.

**Implement**

- Add `board/lp_mspm0c1106/board.mk`, `board.h`, and `board_init.c`.  Board
  headers define clock requirements, pin functions, I2C addresses, and module
  pin ownership.  `board_init.c` derives its masks from those definitions.
- Define the interrupt-priority and nesting policy in `docs/conventions.md` or
  the board configuration before enabling interrupts.  Every SPSC pair names
  its producer, consumer, and their priorities; the plan must be valid under
  Cortex-M pre-emption rather than assuming RH850-style non-nesting.
- Bring up the reset clock first, then the selected run clock.  Expose the
  resulting peripheral-clock values through the clock HAL; no driver may carry
  a private frequency literal.  Every clock-ready wait is bounded; failure
  remains on a documented safe clock, records its cause, and does not hang.
- Implement GPIO output and the `blink` demonstration with a visibly stable
  rate.  Use a timer service for delay timing; do not introduce calibrated busy
  loops.
- Establish a documented OpenOCD or pyOCD path with the selected LaunchPad and
  probe.  Add `make flash` and `make gdb` only after the underlying commands
  have been run successfully from the documented versions.  Documented flash
  commands program MAIN flash only; commands that mass-erase or otherwise
  modify NONMAIN/BCR are excluded until a separate production-security plan
  owns them.
- Implement and host-test `lib_ringbuf`: masked-index SPSC operations,
  capacity/ownership contract, wrap/full/empty tests, and overflow accounting.
  Implement UART TX with that buffer and TX-empty interrupt chaining.  A full
  buffer increments an observable dropped-byte counter; it never silently
  corrupts indices or depends on a periodic one-byte drain.  Expose the count
  through a debugger or UART diagnostic until the Phase 2 debug page exists.
- Add the compile-out `lib_debug`/`DBG_*` macro layer above the UART path;
  drivers and applications do not invent ad-hoc debug output.  Keep UART RX
  disabled in this phase.
- Add timer tick/one-shot primitives, watchdog initialization and kick API,
  `arch_critical_enter/exit`, and an initial `lib_crash` record format/encoder
  used by the fault recorder.  The crash record occupies `.noinit`, contains a
  version and integrity check, and is safe to reject when invalid.  Its design
  explicitly resolves parity/ECC initialisation, reset-cause handling,
  capture/persistence, full-width writes, and safe cold-boot reads; logical CRC
  validation alone is not sufficient.
- Define the image-identity block reserved in Phase 0: fixed linker location,
  format version, firmware version, build/git identifier, image length, and a
  host-generated CRC32 with an explicit coverage rule (for example, the CRC
  field is treated as zero while calculating it).  Patch the ELF as the single
  canonical artifact, then derive HEX and BIN from that patched ELF; `make flash`
  programs the same canonical ELF.  This keeps image-identity fields consistent
  across all distributed artifacts and the flashed target without requiring the
  target's hardware CRC peripheral.
- Add DEBUG-only startup canaries that demonstrate `.data` copying and `.bss`
  zeroing.  Remove neither the underlying startup test nor its documentation
  when debug logging is disabled.

**Bench evidence to record**

- Exact board revision, probe, host OS, tool versions, and `make` command.
- Blink observation, measured/observed UART configuration and boot banner,
  debugger attach/reset, watchdog reset, and the fault-record readback after a
  deliberate test fault and after a cold boot.
- A short reset/reflash recovery procedure in `docs/bringup_lp_mspm0c1106.md`.

**Acceptance gate**

- A clean `make ... flash` programs the target; `make ... gdb` attaches and
  stops in a known symbol after reset, without modifying NONMAIN/BCR.
- The debug build reports the expected startup canaries and a release build
  does not require debug UART to run.
- The recorded raw reset cause distinguishes the tested fault/watchdog/cold-boot
  paths and remains available to application diagnostics after startup.
- Clock-source failure or an invalid clock request reaches the documented safe
  clock state with a recorded failure rather than an unbounded wait.
- Intentional watchdog expiry and intentional fault both reset the MCU and
  leave a validated record available after reboot; the record can also be read
  or safely rejected after a cold boot.
- UART output is lossless at the declared test burst size while the consumer is
  enabled; an overload increments, rather than hides, the drop counter.
- The image identity read back from the flashed target matches the identity
  recorded for the canonical ELF, HEX, and BIN build artifacts.
- CI builds every Phase 1 board/application/variant combination and runs the
  expanded host-test suite.

### Phase 2 — portable libraries and I2C register map

**Goal:** establish the reusable product contract before a product application
is ported.

**Implement in `lib/` first**

- `lib_buildinfo` and `lib_boot`: deterministic version/date identity with BCD
  validation, consistent with the Phase 1 image-identity block.  Decide whether
  build timestamps are reproducible or explicitly supplied by CI; do not make
  two identical source builds differ accidentally.
- `lib_regmap`: table-driven page registration, read-only/read-write rules,
  unmapped reads (`0xFF`), ignored writes to read-only locations, pointer
  auto-increment/wrap, current-address reads, and atomic presentation of
  multi-byte values.  Freeze its API only after the execution model is decided
  per page class: reads are served from published snapshots where required,
  writes/commands are queued for main-loop execution where required, and any
  ISR handler is bounded and explicitly justified by measured timing.  Define
  the exact latch direction and lifetime in both code and protocol documentation.
- Extend `lib_crash` with safe decode and register-map exposure of the Phase 1
  record; the I2C ISR never owns or directly mutates fault state.

**Implement in `hal/`**

- A single `hal_i2c_target` driver that turns I2C events into register-map
  transactions and correctly handles address phase, repeated start, STOP,
  NACK, FIFO/error recovery, and bus reinitialization as required by the TRM.
  Extend the Phase 1 interrupt-priority table with every I2C ISR and the
  snapshot/queue producer-consumer pairs.
- A single instanced `hal_i2c_controller` driver, initially supporting the
  sequence needed by the first application: write, read, and repeated-start
  write-read with explicit timeout and error results, including documented
  stuck-SDA recovery and SCL-low/clock-stretch timeout behaviour.
- The first I2C target configuration in `i2c_regmap_demo`, with a board-defined
  address and standard device-info/debug pages.  Keep board initialization and
  target ownership separate so a future board can use another instance.

**Tooling feasibility**

- Close the C1106 bootloader decision with its actual architecture recorded:
  it has no ROM BSL. A UART/I2C field flow therefore requires a user-owned
  flash BSL at reset address `0x0`, which competes with the application for
  MAIN flash and owns the reset/invocation and recovery policy. Do not add or
  probe a bootloader merely to satisfy this phase. Record the outcome as
  “deferred” unless a product requirement authorizes a separate security,
  partition, authentication, power-loss, and host-tool design.

**Protocol work**

Before declaring compatibility, compare the existing RH850 protocol document
line by line.  Copy compatible semantics into `docs/i2c_register_map.md` and
explicitly resolve its current ambiguities: multi-byte HI/LO/latch behaviour,
pointer effect of short/aborted transactions, write behaviour during a busy
operation, command idempotence, crash-record validity, and which update-page
addresses are reserved rather than implemented.  Back-port agreed protocol
clarifications to the RH850 document in the same change, while recording any
firmware implementation gap there (including its current lack of the proposed
latch-on-HI behaviour) as separate RH850 work.  This phase must not claim a
firmware-update implementation merely because address ranges are reserved.

**Bus characterisation evidence**

Use a documented Raspberry Pi Linux `i2ctransfer` fixture as the initial I2C
master.  Record its exact board/model, OS and `i2c-tools` versions, wiring,
grounding, I/O voltage compatibility, pull-up values/locations, and measured
bus capacitance or justified speed limit.  Any replacement master must be
documented to the same level.  Record the declared supported bus speeds (at
least the intended 100 and/or 400 kHz modes), I2C FIFO depth, measured or
bounded target-service timing, and documented clock-stretch limits.  These
facts justify the register-map execution model and make the timing acceptance
criteria reviewable.

**Acceptance gate**

- Host tests cover every `lib_regmap` access rule and boundary, including
  16-bit pointer `0xFFFF` wrap, read-only writes, sequential transfers,
  current-address reads, torn-read prevention, snapshot publication, queued
  command ordering, and any page-class execution restrictions.
- The on-target target responds to the documented Linux `i2ctransfer` write,
  repeated-start read, and current-address-read commands.  Store the exact
  commands and expected bytes in the bring-up document or a checked-in test
  script.
- Negative tests prove recovery after an early STOP, master NACK, unknown
  address, the documented peripheral error conditions, controller reset during
  a target transaction, stuck SDA (including controller 9-clock recovery), and
  SCL held low until the defined clock-stretch timeout.
- The controller driver demonstrates a repeated-start transaction against a
  known device or controlled test target, with timeout/error paths exercised.
- The C1106 bootloader decision is recorded as either deferred or a separately
  accepted flash-BSL design. Neither outcome modifies NONMAIN/BCR as part of
  this platform phase.
- CI builds the added demo and executes all new host tests.

### Phase 3 — first product-board and application slice

**Goal:** demonstrate that the platform reduces product code to board policy
and application state, rather than reproducing platform work.

**Entry criteria:** Phase 2 is accepted, and the product board schematic,
power sequence, pin assignments, clock requirements, I2C address, product
acceptance behaviour, and a verified programming/debug path are available and
reviewed. The schematic must provide an accessible SWD header; it must not
depend on the LaunchPad's onboard probe. For MSPM0C1106, a BSL/UART path cannot
replace SWD until a separate flash-BSL security design has been approved and
implemented.

**Implement**

- Add the product board directory and only the board-specific initialization,
  pin ownership, and power-sequencing policy it needs.
- Port one bounded product behaviour as an application state machine.  Reuse
  `hal_*` and `lib_regmap`; no application source may duplicate peripheral
  setup or parse raw I2C target events.
- Add application-specific status/control pages beside the shared device-info,
  debug, diagnostic, crash, and UART-drop-count fields.
- Add the needed peripheral HAL only when the product requires it.  ADC work,
  for example, includes invalid-sample semantics and host-tested conversion
  math; it is not added as a speculative driver.
- Produce `docs/bringup_<board>.md` with wiring, reset/power ordering, expected
  boot output, flash/debug procedure, I2C examples, known limits, and pass/fail
  checks.

**Acceptance gate**

- The application builds in debug and release configurations through the same
  top-level command interface as `blink`.
- Product acceptance behaviour has at least one automated host test where
  practical and one executable, documented bench procedure.
- The standard register-map fields work with the existing Pi-oriented host
  commands after only the board address/bus choice changes.
- Static-analysis findings are either fixed or listed in a project deviation
  log with rule, scope, justification, and mitigation.
- CI adds the product board/application and its host tests to the established
  build matrix.

### Phase 4 — quality automation and release baseline

**Goal:** make regressions visible before hardware testing, and make the first
platform release reproducible.

**Implement**

- Extend the Phase 0 CI workflow to the complete supported `BOARD x APP x
  {debug,release}` matrix, artifact/map inspection, and release-quality
  formatting/test gates.  The minimal checks remain mandatory throughout;
  Phase 4 does not replace them.
- Run cppcheck with the MISRA addon, clang-tidy, and GCC `-fanalyzer` where
  their versions support the selected source set.  Check in the command
  versions, suppressions, and formal deviation log; fail CI on an unreviewed
  increase from the approved baseline.
- Add `tools/size_report.py` and per-board/app flash/RAM budgets.  A budget
  breach fails rather than merely prints.  Stack-usage files are collected as
  diagnostics; only make a strict whole-program stack claim once a documented
  call-graph method supports it.
- Add `make info`, `make size`, `make misra`, `make test`, `make clean`, and
  `make clean-all` with documented behaviour.  Clean commands remove only
  repository build outputs.
- Add release documentation: supported board matrix, exact toolchain version,
  known-good probe path, source/licence inventory, test evidence, and known
  hardware/tool limitations.

**Acceptance gate**

- CI is green from a clean checkout and fails when a source-format error, host
  test failure, unsupported board/app pair, new warning, dependency omission,
  or size-budget overrun is injected.
- CI rejects a compiler version other than the pinned toolchain release.
- The release build can be reproduced locally from its documented toolchain
  version; its image identity and section sizes are archived with the release.
- A reviewer can execute all documented MSPM0 acceptance commands without
  needing an undocumented SDK installation or file outside the repository.

### Phase 5 — MSPM33C321 device port

**Goal:** validate that the shared architecture works on the second family
without contaminating applications or libraries with device conditionals.

**Entry criteria:** MSPM33 datasheet/TRM/errata, SDK licence, reference board,
and a viable debug/flash path have been verified in `device_facts.md`.  If any
one is unavailable, retain MSPM33 as a tracked risk rather than adding
untested placeholder code.

**Implement**

- Add vendored device definitions, `device/mspm33c321/`, the device Make
  fragment, and an MSPM33 reference board.  Start with an M33 image in the
  single secure state described in the architecture proposal.
- Add `arch_cm33.c` only for genuine architectural differences, such as FPU
  enablement when verified.  Do not set hard-float flags until the exact core,
  FPU, ABI, and library support are evidenced.
- Bring up, in order: startup/linker validation, blink, clock, debugger/flash,
  UART, timer/watchdog/fault path, I2C target, and controller.
- Compare peripheral register/IP behaviour with MSPM0.  Put a genuine
  divergence behind a narrow `hal/mspm33/` backend boundary; otherwise keep a
  shared driver.  Record the evidence for every split.

**Acceptance gate**

- All Phase 1 and Phase 2 MSPM33 demonstrations pass on its reference board.
- Existing MSPM0 source builds and tests stay green with no M33-only source
  conditionals in `app/` or `lib/`.
- The port review reports changed lines outside `ti/`, `device/`, `arch/`,
  `board/`, and justified HAL backends.  A large unexplained change is an
  architectural issue to resolve before proceeding.

### Phase 6 — future decisions after two working ports

Only after both device paths are stable should the team decide whether to
extract files shared with the RH850 repository.  Compare actual API churn,
release cadence, testing burden, licence compatibility, and consumer needs.
Choose one of:

1. Keep deliberate manual parity, with documented source provenance and
   optional CI hash/content checks for selected files.
2. Use a git subtree for a small, stable, licence-compatible library/protocol
   set.

Do not create a generic third firmware framework merely to avoid a small
amount of duplicated code.  TrustZone/non-secure images, production security
configuration, BSL field flashing, HIL CI, update bootloaders, and added
peripherals each require their own approved design and delivery plan.

## 5. Initial file and ownership map

The following sequence makes review scope clear.  An item should be added only
when its phase needs it.

| Area | Initial responsibility | First phase |
|---|---|---:|
| `Makefile`, `make/` | build selection, flags, dependency/artifact rules | 0 |
| `ti/` | minimal upstream headers/SVD, version and licence inventory | 0 |
| `device/mspm0c1106/` | linker script, startup, device wrapper | 0 |
| `arch/` | critical sections, barriers, fault record interface | 1 |
| `board/lp_mspm0c1106/` stub | device selection and a minimal public board header | 0 |
| `board/lp_mspm0c1106/` | board clock/pin/power policy | 1 |
| `hal/hal_clock`, `hal_gpio`, `hal_uart`, `hal_timer`, `hal_wdt` | first hardware services | 1 |
| `lib_ringbuf`, `lib_debug`, `lib_crash` format/encode | UART SPSC contract, debug layer, fault record | 1 |
| `lib_buildinfo`, `lib_boot`, `lib_regmap`, `lib_crash` decode/page | image identity and portable register-map services | 2 |
| `hal/hal_i2c_target`, `hal_i2c_controller` | instanced I2C services | 2 |
| `app/blink` | link-complete placeholder (Phase 0), then physical GPIO smoke test (Phase 1) | 0–1 |
| `app/i2c_regmap_demo` | protocol demonstration | 2 |
| `tests/` | host tests and test runner | 0 onward |
| `tools/` | only verified flash/debug/BSL/size tooling | 1, 2, 4 |
| `.github/workflows/ci.yml` | minimal build/test/format CI, expanded each phase | 0 onward |
| `device/mspm33c321/`, `board/<m33-board>/` | second-device port | 5 |

## 6. Test strategy and evidence model

Testing is layered so a failure identifies its likely owner quickly.

| Layer | Runs where | Proves | Required examples |
|---|---|---|---|
| Library unit tests | native host compiler | deterministic protocol/data logic without host-width or UB blind spots | `stdint.h`-only interfaces; `-Wconversion`; ASan+UBSan; 32-bit build where available; ring-buffer wrap/overflow; regmap snapshot/queue and latch rules; BCD/build-info validity; conversion math |
| Cross build | CI and developer host | all selected source combinations compile and link | board/application/variant matrix; dependency tracking; output paths |
| Static analysis | CI and developer host | defined coding-policy baseline | compiler warnings as errors, formatter, cppcheck/MISRA, clang-tidy, analyzer findings |
| Image inspection | CI and developer host | image fits and starts as designed | map regions, vector placement, `.data` LMA/VMA, `.bss`/`.noinit`, stack/flash/RAM budgets |
| Board smoke | reference board | physical startup and core HAL work | flash, debugger attach, blink, UART, timer, watchdog, fault reset |
| Bus integration | reference board plus documented Raspberry Pi I2C fixture | wire protocol, timing, and bus recovery | address write/read/restart/current-address; error/abort behaviour; target reset mid-transfer; stuck-SDA recovery; SCL-low timeout; controller transaction; recorded wiring, voltage, pull-ups, and tool versions |
| Product acceptance | product board | end-user observable behaviour | board power sequence, device-specific I2C pages, fault/recovery behaviour |

Every hardware observation should record the firmware image identity, board
revision, relevant tool versions, command, expected result, actual result, and
date.  The aim is reproducibility, not a large manual test report.

## 7. Technical decisions to make at implementation time

These are deliberately deferred decisions with required evidence, not missing
design work.

| Decision | Owner/evidence | Deadline |
|---|---|---|
| Exact GNU Arm release, newlib-nano/linker specifications, warning flags, and version-enforcement rule | clean local/CI build, licence review | Phase 0 |
| Header/SVD source, version, scope, and redistribution notice | official package and legal/licence review | before `ti/` is committed |
| Host-test framework (Unity or checked-in assert harness) and sanitizer/32-bit availability policy | clean host and CI test runs | Phase 0 |
| Reference-board name, revision, LEDs, UART bridge, I2C pins/address, probe | board schematic/manual plus bench check | Phase 1 |
| Flash/debug tool, minimum version, and verified MAIN-only erase/program scope | successful clean flash/reset/debug session plus NONMAIN/BCR review | Phase 1 |
| Startup SRAM/ECC/parity sequence, memory map, raw reset-cause capture/clearing semantics, `.noinit` lifetime, and safe cold-boot read policy | device facts, reset-cause review, map/canary evidence, and bench fault test | Phase 0–1 |
| Exact fault-record fields/checksum, full-word write policy, and reset policy | fault-handler review plus warm- and cold-boot deliberate-fault tests | Phase 1 |
| Interrupt-priority/nesting map and every SPSC producer/consumer priority pairing | Cortex-M priority-bit facts and concurrency review | Phase 1, extended in Phase 2 |
| Fixed image-identity block: linker location, format, version/build identifier, length, host-side CRC32 coverage rule, and canonical patched-ELF workflow | linker/post-build design, artifact comparison, flash read-back, and reproducibility test | Phase 1 before Phase 2 device-info freeze |
| I2C target IRQ/FIFO/recovery state machine | TRM/errata plus negative bus tests | Phase 2 |
| Register-map execution context per page class (ISR vs published snapshot plus queued main-loop work) | FIFO/clock-stretch limits, worst-case handler timing, concurrency review, and host tests | Phase 2 before `lib_regmap` API freeze |
| Register-map latch and busy-operation semantics | RH850 compatibility review and host tests | Phase 2 |
| C1106 field-update bootloader and product-board invocation/debug path | documented defer decision, or a separately reviewed flash-BSL design and product-board schematic review | Phase 2–3 |
| Product peripherals and first product acceptance suite | product schematic/requirements | Phase 3 |
| CI package versions and static-analysis baseline | reproducible CI runs | Phase 4 |
| M33 FPU/TrustZone/compiler flags and HAL backend splits | M33 source documentation and board evidence | Phase 5 |
| Shared-code extraction | two-port maintenance data | Phase 6 |

## 8. Risks and response rules

| Risk | Early indicator | Response |
|---|---|---|
| MSPM33 SDK, reference-board, or open debug support is not mature | no documented successful flash/debug path | do not block MSPM0 delivery; retain one-repo structure and revisit at Phase 5 entry |
| A proposal assumption differs from the device documentation | conflict found in `device_facts.md` | update the fact record and affected design document before coding; do not preserve an assumption for plan consistency |
| Vendor licence forbids intended vendoring | unclear redistribution terms | use a documented external dependency or alternate permitted package; do not commit unlicensed SDK content |
| A flash/debug tool erases or writes MSPM0 NONMAIN/BCR | a documented command includes non-main address ranges, mass erase, or BCR option | stop; remove the command from normal tooling and treat it as production-security scope with an explicit recovery analysis |
| I2C target behaviour is more complex than the common abstraction | errata/FIFO/error tests require device-specific sequencing | keep common public API; add the smallest evidence-backed family backend |
| Product delivery pressures add hardware-specific calls to apps | direct register/header includes appear in app code | stop review, move policy to board/HAL, and add the regression to the build matrix |
| Static-analysis output becomes noise | growing unactioned suppressions | treat deviations as reviewed records; avoid blanket suppressions and baseline increases |
| Firmware image outgrows memory | size trend/budget warning | fail CI at the budget; optimize deliberately or revise hardware requirement with recorded approval |
| Field update/security is needed early | BSL lock/protection or product requirement emerges | create a separate threat model and bootloader/update plan before reserving or writing production flash regions |

## 9. Definition of done for the initial platform release

The MSPM0 release is ready to serve as a coding platform when all applicable
Phase 0–4 gates pass and the repository includes:

- a supported-board and supported-application matrix;
- device facts with source references and no unresolved critical entry for the
  MSPM0 reference board;
- reproducible build, flash, debug, test, analysis, and size-report commands;
- owned startup/linker/fault code evidenced on real hardware;
- a host-tested register-map library and a bench-tested I2C target demo;
- a CI result for the released revision plus a concise release/bring-up record;
- licence notices, tool versions, known limitations, and future work clearly
  documented.

MSPM33 support becomes a supported platform only after it independently meets
the equivalent Phase 5 gate.  A directory, a compile-only target, or an
unverified device header is not support.
