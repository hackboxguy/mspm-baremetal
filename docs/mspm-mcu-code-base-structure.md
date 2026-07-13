# MSPM0 / MSPM33 Bare-Metal Code Base — Architecture Proposal (v1)

**Author:** Claude (Fable 5)
**Date:** 2026-07-07
**Targets:** TI MSPM0C1106 (Arm Cortex-M0+) and MSPM33C321 (Arm Cortex-M33), automotive (-Q1) variants
**Context:** Companion proposal to the existing `rh850-baremetal-demo` repo (Renesas RH850/F1KM-S1, CC-RH). Goal: same developer experience (`make BOARD=… APP=…`, terminal-only, modular app/board/hal/lib layout), but built entirely on **open-source tooling**, and with the RH850 review findings (`fable-rh850-review-v1.md`) baked in as design rules instead of repeated mistakes.

> **Note on device facts:** MSPM33 is a new TI family and my parameter knowledge of MSPM33C321 is thin; MSPM0C1106 is a recent C-series part. Items marked *(verify)* must be checked against the current datasheet/TRM before they become constants in `device/`.

---

## 1. The two repo questions — answered up front

### 1.1 Extend the RH850 repo, or create a new repo? → **New, separate repo.**

| Consideration | RH850 repo | MSPM repo | Verdict |
|---|---|---|---|
| Compiler | CC-RH, proprietary, licensed, C89/C90, vendor types (`uint8`), `#pragma interrupt`, WSL `/tmp` copy hack | `arm-none-eabi-gcc` (free), C11, `stdint.h`, CMSIS attributes, native paths | Nothing in the build system transfers; forcing both into one Makefile means `ifeq` soup everywhere |
| Language baseline | Frozen at C89 by CC-RH (declarations-at-top, no `//`…) | C11, designated initializers, `static_assert`, inline | A shared tree would drag the ARM code down to C89 or fork every file anyway |
| Device headers / licensing | Renesas vendor header, Renesas license terms | TI MSPM0 SDK headers (BSD-3-Clause), CMSIS (Apache-2.0) | Cleaner to keep third-party license trees per vendor |
| Flash/debug flow | Renesas flasher on Pi (`flashrh850.sh`), no on-chip debug in workflow | OpenOCD/pyOCD + GDB, TI ROM bootstrap loader (BSL) over UART | Different `tools/` entirely |
| CI | Blocked by CC-RH license | Free toolchain → full CI possible | You don't want the un-CI-able half poisoning the CI-able half |
| Stability | `main` is a tested, shipping baseline | New, will churn heavily | Don't destabilize a hardware-validated repo with an unrelated port |

**Recommendation:** new repo, e.g. `mspm-baremetal` (or `ti-mspm-baremetal`), that **mirrors the RH850 repo's shape and naming conventions** (`app/`, `board/`, `hal/`, `lib/`, `tools/`, `docs/`, same Make UX, same I2C register-map protocol). Knowledge and muscle memory transfer; code and build systems don't have to.

What *does* get shared across the two repos (see §8): the portable `lib/` components (ring buffer, build info, register-map framework), the **I2C slave protocol spec** (so the same Pi-side `i2ctransfer` scripts and host tooling work against RH850 and MSPM units), and the docs/golden-verification culture. Start by copying with 1:1 file parity; promote to a `git subtree` (not submodule — subtree keeps clones trivial) only when both repos have proven the API is actually stable. Do **not** build a third "common firmware framework" repo on day one — that's premature abstraction with two data points.

### 1.2 One repo for MSPM0 + MSPM33, or two? → **One common repo.**

| Consideration | Detail |
|---|---|
| Same toolchain | Both build with `arm-none-eabi-gcc`; only `-mcpu=cortex-m0plus` vs `-mcpu=cortex-m33` (+FPU flags) differ — one variable in a per-device makefile fragment |
| Same vendor SDK style | MSPM33 is TI's scale-up of MSPM0; TI positions them with a shared SDK/driverlib register-access idiom *(verify how much peripheral IP is actually shared — SYSCTL/GPIO/UART/I2C register maps may be identical or near-identical)* |
| Same board/app model | Your `BOARD` already implies the MCU; adding `DEVICE` as a board attribute is one line in `board/<X>/board.mk` |
| Shared HAL surface | The HAL API (`hal_uart_*`, `hal_i2c_target_*`, …) is identical; at most the *backend* differs per family — exactly the abstraction the repo exists to provide |
| Divergence risks | Cortex-M33 extras (TrustZone, MPU, FPU, more NVIC priority bits, possibly cache/ECC) are additive and live in `arch/` + `device/`, not in apps |

Two repos would duplicate the Makefile, `lib/`, HAL API, docs, and CI within weeks — the same copy-paste divergence the RH850 review flagged *inside one repo* (four near-identical I2C drivers), but at repo scale. The only scenario justifying a split later is if MSPM33 work becomes a TrustZone-heavy secure-boot platform with a fundamentally different build (secure + non-secure images, CMSE veneers). Design for that as a *build dimension you don't use yet* (§5.4), not as a separate repo.

**Resulting picture: 2 repos total** — `rh850-baremetal-demo` (as-is) and `mspm-baremetal` (MSPM0 + MSPM33 together).

---

## 2. Target hardware snapshot

| Item | MSPM0C1106 | MSPM33C321 |
|---|---|---|
| Core | Cortex-M0+ (ARMv6-M), no FPU, no hardware divide | Cortex-M33 (ARMv8-M Mainline), FPU + DSP *(verify)*, optional TrustZone |
| Clock | Internal SYSOSC ~24 MHz class *(verify exact max/PLL options)* | Higher *(verify — likely 80-160 MHz class with PLL)* |
| Flash/SRAM | *(verify — C-series, tens of KB flash, several KB SRAM; check ECC/parity)* | *(verify — larger; automotive parts usually flash ECC + SRAM ECC)* |
| NVIC priority bits | 2 (4 levels) | ≥3 *(verify)* |
| Peripherals of interest | UART, I2C (controller **and** target mode with FIFOs), TIMG/TIMA timers, 12-bit ADC, WWDT, DMA, GPIO, CRC *(verify per-part instance counts)* | Superset; check CAN-FD/LIN availability for the automotive use case *(verify)* |
| Safety/quality | AEC-Q100 (-Q1); TI functional-safety collateral (FIT rates, safety manuals) *(verify tier)* | Same, likely higher safety tier *(verify)* |
| Debug | SWD; XDS110 / J-Link / any CMSIS-DAP probe | SWD (+ TrustZone-aware debug) |
| ROM bootloader | BSL over UART/I2C (documented protocol, invokable via pin or blank-flash) | Expected equivalent *(verify protocol compatibility with MSPM0 BSL)* |

Two facts matter architecturally: **the I2C peripheral has a proper target (slave) mode with FIFOs** — your EEPROM-style 16-bit register-map protocol ports directly and more comfortably than on RIIC; and **M0+ has no FPU and 4 interrupt priority levels** — the lowest common denominator the shared HAL must respect.

---

## 3. Repository layout

```
mspm-baremetal/
├── Makefile                      # Thin: parses BOARD/APP/DEBUG/VERSION, includes make/*.mk
├── make/
│   ├── toolchain.mk              # arm-none-eabi-* discovery, pinned flags, warnings
│   ├── rules.mk                  # Pattern rules, dependency tracking, build dirs
│   ├── device_mspm0c1106.mk      # -mcpu=cortex-m0plus, device defines, linker script path
│   └── device_mspm33c321.mk      # -mcpu=cortex-m33 -mfpu=... -mfloat-abi=hard
├── ti/                           # Vendored third-party code, license headers preserved
│   ├── LICENSE.txt               # TI BSD-3-Clause + CMSIS Apache-2.0 notices
│   ├── cmsis/                    # core_cm0plus.h, core_cm33.h, cmsis_gcc.h (subset)
│   ├── mspm0c1106/               # Device header(s) + SVD from MSPM0 SDK
│   └── mspm33c321/               # Same for MSPM33 (from its SDK when available)
├── device/                       # OUR startup + memory layout (small, reviewed, owned)
│   ├── mspm0c1106/
│   │   ├── startup.c             # Vector table, Reset_Handler, .data copy, .bss zero, RAM clear
│   │   ├── linker.ld             # Flash/RAM regions, .data AT> FLASH, noinit section, stack guard
│   │   └── device.h              # Includes TI header, adds device-level constants
│   └── mspm33c321/               # + optional TrustZone partitioning (unused initially)
├── arch/                         # Core-architecture helpers (no peripheral knowledge)
│   ├── arch.h                    # critical_enter/exit, barriers, WFI wrapper, cycle counter
│   ├── arch_cm0plus.c
│   ├── arch_cm33.c
│   └── fault.c                   # HardFault etc. → crash record in .noinit + WDT reset
├── hal/                          # Common API, per-family backend where register IP differs
│   ├── hal_clock.[ch]
│   ├── hal_gpio.[ch]
│   ├── hal_uart.[ch]             # TX blocking + TX ring-buffer (IRQ-driven, not tick-drained)
│   ├── hal_i2c_controller.[ch]   # One driver, N instances (bus handle) — no per-bus copies!
│   ├── hal_i2c_target.[ch]       # 16-bit sub-addressed slave, callback API as on RH850
│   ├── hal_timer.[ch]            # 1 ms tick + one-shot/us-delay service
│   ├── hal_adc.[ch]
│   ├── hal_wdt.[ch]              # Watchdog: init + kick; ON by default in every app
│   ├── hal_crc.[ch]              # HW CRC (image check, golden tests)
│   └── mspm0/… , mspm33/…        # Backends, only if the register IP actually differs
├── lib/                          # Portable, host-testable, no register access
│   ├── lib_ringbuf.[ch]          # Ported from RH850 (with the index-masking fix)
│   ├── lib_debug.h               # DBG_* macros → UART ring buffer; compile out in release
│   ├── lib_buildinfo.h           # BCD build date/time (port as-is)
│   ├── lib_regmap.[ch]           # NEW: shared I2C register-map framework (pages, RO/RW,
│   │                             #      16-bit latch-on-HI-read, common device-info page)
│   ├── lib_boot.[ch]             # Boot banner
│   └── lib_crash.[ch]            # Crash record encode/decode (fault.c ↔ debug page)
├── board/
│   ├── <BOARD_A>/                # e.g. first MSPM0C1106 board
│   │   ├── board.mk              # DEVICE := mspm0c1106
│   │   ├── board.h               # Pins, clocks, I2C addresses, feature flags
│   │   └── board_init.c          # Pinmux + power sequencing
│   └── <BOARD_B>/                # e.g. first MSPM33C321 board
├── app/
│   ├── blink/                    # Smoke test (every device bring-up starts here)
│   ├── i2c_regmap_demo/          # The i2c_slave equivalent, built on lib_regmap
│   └── <product_app>/            # display_manager/983_manager equivalents
├── tests/                        # Host-side unit tests (gcc native + Unity or plain asserts)
│   ├── test_ringbuf.c
│   ├── test_regmap.c
│   └── Makefile
├── tools/
│   ├── flash_openocd.sh          # make flash → OpenOCD (XDS110/CMSIS-DAP/J-Link)
│   ├── bsl_flash.py              # UART ROM-bootloader flashing (Pi field flow, like flashrh850.sh)
│   ├── gdbinit                   # make gdb → openocd + arm-none-eabi-gdb attach
│   └── size_report.py            # Section sizes + diff vs committed baseline
├── docs/
│   ├── i2c_register_map.md       # THE protocol spec — kept byte-compatible with RH850 repo
│   ├── bringup_<board>.md
│   └── conventions.md            # Coding rules distilled from the RH850 review
├── .github/workflows/ci.yml      # Build matrix + cppcheck/MISRA + host tests + size diff
├── CLAUDE.md                     # Same role as in the RH850 repo
└── README.md
```

Deliberate differences from the RH850 repo, each traceable to a review finding:

| RH850 review finding | Design rule here |
|---|---|
| B1: `.data` never copied to RAM (no `-rom=`) | GNU LD linker script with `.data : AT(...)` + hand-owned `startup.c` copy loop; **boot self-check**: a `static const`-pattern `.data`/`.bss` canary verified in `main()` on DEBUG builds |
| S5/Learning #5: "BSS may not be zeroed" folklore | Startup zeroes `.bss` *and* optionally clears all SRAM (ECC/parity init) before `main`; documented, tested |
| B2: board_init clobbered UART pins | `board.h` declares *owned pin sets* per module; `board_init.c` masks are generated from those defines, not hand-copied hex |
| B3/dup drivers: 4 copy-paste I2C drivers | **One** controller driver + **one** target driver, instance handles (`hal_i2c_controller_open(I2C0)`) — MSPM0 peripherals are memory-mapped instances, this is natural |
| B4-B6: ring buffer contract violations, never drained | UART TX is IRQ-driven (TX-empty interrupt chains bytes) — no 1 ms tick drain, no 1 byte/ms ceiling; ring buffer gets masked indices + a dropped-byte counter exposed on the debug page |
| B7: torn 16-bit reads | `lib_regmap` implements latch-on-HI-read for all 16/32-bit values as framework behavior, not per-app discipline |
| §3.1: no watchdog | `hal_wdt` kicked from the main loop in the app template; default handlers = crash record + reset, never `while(1){}` |
| §3.3: silent exception loops | `arch/fault.c` stores EPC/xPSR/fault status into `.noinit` RAM; readable after reset via the debug page; DEBUG builds print it at boot |
| §4.1: no header dep tracking | `-MMD -MP` + `-include $(DEPS)` from day one |
| §4.2: flattened object names | Object tree mirrors source tree (`build/<board>_<app>_<variant>/hal/hal_uart.o`) |
| §4.3: unpinned flags | Pinned: `-std=c11 -Os -g3 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fno-common -fstack-usage`, `-Wl,--gc-sections,-Map=...`; delay loops replaced by SysTick/timer-based `hal_delay_us()` — no compiler-dependent busy-wait constants |
| §5.1: tool depends on file outside repo | Everything the build/tools need lives in-repo; external inputs (if any) are hash-pinned |
| §5.2: no CI | Free toolchain → full CI (see §7) |

---

## 4. Toolchain — all open source

| Function | Primary choice | Notes / alternatives |
|---|---|---|
| Compiler/linker | **Arm GNU Toolchain** (`arm-none-eabi-gcc`) | Debian/Ubuntu package or xPack release; pin the version in `make/toolchain.mk` and CI. Alternative: LLVM/Clang (`--target=arm-none-eabi`) kept working as a second compiler for extra diagnostics, not as the shipping compiler |
| C library | newlib-nano (`--specs=nano.specs --specs=nosys.specs`) | No heap by default; `printf` avoided in firmware (keep the RH850-style `hex8/hex32` putters) |
| Build | **GNU Make** (your preference) | Structured as thin top Makefile + `make/*.mk` fragments so it stays readable; CMake/Meson deliberately not used |
| Flash (bench) | **OpenOCD** (MSPM0 support is upstream — *verify the minimum version; MSPM33 support timing TBD*) | pyOCD (via TI CMSIS-Packs) and probe-rs as alternatives; TI UniFlash/dslite works but is closed-source — keep it out of the required path |
| Flash (field/Pi) | **`tools/bsl_flash.py`** — TI ROM bootstrap loader over UART | Replaces the `flashrh850.sh` role: Pi toggles BSL-invoke + NRST GPIOs, streams the image over `/dev/ttyS0`. BSL protocol is documented by TI; implement in ~300 lines of Python, no proprietary tools on the Pi |
| Debug | `arm-none-eabi-gdb` + OpenOCD (`make gdb`) | Any CMSIS-DAP probe; XDS110 on TI LaunchPads works with OpenOCD/pyOCD |
| Static analysis | **cppcheck + MISRA addon** (same flow as RH850 → your deviation-log process ports as-is) + `clang-tidy` + GCC `-fanalyzer` | Three free analyzers ≫ one; MISRA deviation log format copied from the RH850 repo |
| Unit tests | Host `gcc` + **Unity** (or plain assert harness) for `lib/` and pure logic | This is what was impossible to retrofit on RH850; here `lib/` is designed register-free so it compiles on the host unmodified |
| Formatting | `clang-format` with a checked-in style file | Ends whitespace churn in reviews |
| Emulation (optional) | Renode has TI Cortex-M platforms and is extensible | Nice-to-have for CI smoke tests; don't block on it |

LaunchPads exist for MSPM0 (and presumably MSPM33 *(verify)*) — budget one per family as the CI-adjacent reference board before custom hardware arrives; give each a `board/` entry (`board/lp_mspm0c1106/`).

---

## 5. Key architecture decisions

### 5.1 Own thin HAL over vendored TI headers (same philosophy as RH850)

Vendor **only** the register-definition headers + SVD from TI's SDK into `ti/` (BSD-3-Clause — license-compatible with your MIT repo; keep their notices). Write the HAL yourself, as you did for RH850 — you clearly value understanding every register write, and the review showed that's where your team's strength is. TI's driverlib is mostly thin static-inline wrappers; it's fine for HAL internals to call the occasional inline where it saves datasheet-diving, but **apps never touch `ti/` or registers directly** — that boundary is the rule that keeps apps portable between MSPM0 and MSPM33 (and conceptually, RH850).

The HAL API should be signature-compatible with the RH850 HAL where it's sane (`hal_uart_puts`, `hal_i2c_target_init(addr, on_write, on_read)`, `hal_timer_init(period_us, cb)`), so app logic ports between the repos by recompiling, not rewriting.

### 5.2 The I2C register-map protocol is the cross-repo contract

The most valuable reusable asset you own isn't code — it's the **EEPROM-style 16-bit register map** (device-info page at 0x0000, status 0x0100, control 0x0200, debug 0x0300, diagnostics 0x1000, FW update 0xF000). Keep `docs/i2c_register_map.md` byte-identical in intent across both repos so:

- the same Pi test scripts (`i2ctransfer …`) exercise RH850 and MSPM units,
- host-side production/EOL tooling is device-agnostic,
- the planned FW-update page (0xF000) gets **one** design that both platforms implement.

New here: implement it once as `lib_regmap` (table-driven pages, RO/RW enforcement, multi-byte latch semantics, common device-info page served by the framework) instead of per-app `switch` statements — this is the §6.4 dedup suggestion from the review, done from day one.

### 5.3 M0+ is the floor; M33 features are additive

Shared code assumes ARMv6-M: no FPU, no hardware divide (beware `/` and `%` in hot ISRs on MSPM0 — the RH850 NTC integer math ports fine but check the division in the Beta equation), 4 NVIC priority levels, no exclusive-access instructions (`LDREX/STREX`) — so `arch_critical_enter/exit` uses PRIMASK, and the SPSC ring buffer stays the lock-free primitive (single-core, in-order on both parts — the RH850 reasoning holds). M33 extras live behind `arch/`:

- FPU: enabled in `arch_cm33.c` (CPACR) + lazy stacking; `-mfloat-abi=hard` only in the M33 device fragment.
- MPU: optional later (stack guard region is cheap and worth it).
- **TrustZone: keep it off / secure-only initially.** Build the M33 image as a single secure-world image with SAU untouched. If a real secure-boot requirement lands, that becomes a new build dimension (`SECURE=`/`NONSECURE=` targets + CMSE veneer lib) inside this same repo — the layout above doesn't need to change, `device/mspm33c321/` grows a partition file.

### 5.4 Build UX (kept deliberately identical to RH850)

```bash
make BOARD=lp_mspm0c1106 APP=blink
make BOARD=<board> APP=i2c_regmap_demo DEBUG=on VERSION=01.02
make BOARD=<board> APP=<app> flash          # OpenOCD, bench probe
make BOARD=<board> APP=<app> gdb            # attach debugger
make BOARD=<board> APP=<app> size           # section sizes vs flash/RAM budget
make misra                                   # cppcheck MISRA, deviation-log workflow
make test                                    # host unit tests (native gcc)
make clean / clean-all / info
```

Board selects device (`board.mk: DEVICE := mspm0c1106`); apps declare nothing about the MCU. `VERSION` gets format validation (review §4.4). Outputs: `.elf`, `.map`, `.bin`, `.hex` + `crc32` stamped into a fixed flash location (image identity — feeds the future A/B bootloader on both platforms).

### 5.5 Startup & memory correctness (the B1 lesson, institutionalized)

`device/*/linker.ld` and `startup.c` are **owned, small, and reviewed** — not copied opaquely from an SDK:

- `.data` with proper `AT> FLASH` load address; startup copies LMA→VMA; `.bss` zeroed; both verified by canary globals in DEBUG builds.
- `.noinit` section for the crash record and reboot-reason (survives WDT reset).
- Optional whole-SRAM clear before `.data` copy (ECC/parity init on automotive parts *(verify which of these two devices need it)*).
- Stack placed with a guard pattern; `make size` reports worst-case static stack from `-fstack-usage` + call-graph script (no more "4 KB because 512 crashed once").
- Vector table in flash; every unused vector points at the fault recorder, not at `while(1)`.

---

## 6. Phased bring-up plan

1. **Phase 0 — skeleton (no hardware):** repo layout, `make/` fragments, vendored MSPM0C1106 headers, linker script + startup, `blink` app compiles, host tests + CI green. *(1-2 days)*
2. **Phase 1 — MSPM0C1106 LaunchPad bring-up:** clock, GPIO, UART (banner + IRQ TX ring), SysTick 1 ms, watchdog, fault recorder. `make flash`/`gdb` working; `bsl_flash.py` against the LaunchPad's BSL. *(≈1 week)*
3. **Phase 2 — the I2C core:** `hal_i2c_target` + `lib_regmap` → `i2c_regmap_demo` answering the standard Pi `i2ctransfer` suite (reuse the RH850 test commands verbatim as the acceptance test); `hal_i2c_controller` with repeated-start write-read; ADC + temperature page if the first product needs it. *(1-2 weeks)*
4. **Phase 3 — first product app + custom board:** port the relevant product logic (display-manager-style state machine ports almost mechanically once HAL and regmap exist).
5. **Phase 4 — MSPM33C321:** add `ti/mspm33c321/` + `device/mspm33c321/` + `make/device_mspm33c321.mk`, bring up `blink` → UART → I2C on its LaunchPad. This phase is the test of the architecture: **target ≤ a few dozen changed lines outside `device/`/`ti/`/`board/`.** Divergences discovered here decide whether any `hal/mspm33/` backends are needed.
6. **Phase 5 — shared-lib consolidation:** once `lib_regmap`/`lib_ringbuf` are stable in both repos, decide on `git subtree` extraction (or accept manual parity — with CI-checked file hashes — if churn is low).

---

## 7. CI (the big win over the RH850 setup)

`arm-none-eabi-gcc` is apt-installable, so GitHub Actions (or any runner) can do what CC-RH's license never allowed:

- **Build matrix:** every `BOARD × APP × {release, debug}` — catches the "board_init evolved, old app silently broke" class (RH850 finding B2) on every PR.
- **Host unit tests** for `lib/` (ring buffer, regmap semantics incl. latch-on-HI-read, buildinfo BCD, NTC math vs float reference).
- **cppcheck MISRA** with the count pinned (fail on increase), same deviation-log process as RH850.
- **Size regression:** `size_report.py` diffs section sizes vs a committed baseline (the golden-reference idea from `check_983_manager.py`, generalized).
- **Format check** (`clang-format --dry-run -Werror`).
- Optional later: HIL stage — a self-hosted Pi runner that BSL-flashes a LaunchPad and runs the `i2ctransfer` acceptance suite. Your Pi-centric field flow makes this unusually cheap to build.

---

## 8. What to port from the RH850 repo (and what to fix while porting)

| Asset | Action |
|---|---|
| `lib_ringbuf` | Port; **fix unmasked index** (review B5); add dropped-byte counter; keep SPSC contract but document it in the header with the producer/consumer assignment per app |
| `lib_buildinfo.h` | Port as-is (pure preprocessor) |
| `lib_debug.h` | Port; add `DBG_HEX32` for real; route to IRQ-driven UART TX |
| I2C register-map spec (`docs/i2c_register_map.md`) | Port as the canonical spec; add the clarifications the review asked for (HI/LO ordering, busy-window NACK semantics, idempotent-read rule) and back-port those doc fixes to the RH850 repo |
| Boot banner / `BOOT_BANNER()` | Port (trivial) |
| Golden-baseline + generator verification pattern (`check_983_manager.py` philosophy) | Reuse the *pattern* wherever generated data appears (regmap tables, panel profiles if MSPM ever drives displays); keep all inputs in-repo, hash-pinned (review §5.1) |
| MISRA deviation-log format (`misra_deviations.md`) | Port the process and template |
| CLAUDE.md "Critical Learnings" culture | Port the habit; seed it with the §3 design-rules table above so the new repo starts with lessons pre-learned |
| NTC integer temperature math | Port `approx_ln_x10000`/`adc_to_temp_degc10` into `lib/` **with a host unit test** against `math.h` reference; add invalid-sample sentinel (review B8) |

Explicitly **not** ported: the timer-tick UART drain architecture (replaced by TX-interrupt chaining), per-bus copy-paste I2C drivers (replaced by instanced driver), busy-wait delay constants (replaced by timer-based delays), init-globals-in-main convention (obsolete once startup is correct).

---

## 9. Risks / open items

1. **MSPM33 maturity** — new family: confirm SDK availability, OpenOCD/pyOCD support, BSL protocol, errata volume, and LaunchPad availability before committing schedules. If tooling is immature, Phases 0-3 on MSPM0 lose nothing — that's part of why one repo with a device abstraction is the right shape.
2. **Device-fact verification** — every *(verify)* in §2 before `device/` constants are written; keep a `docs/device_facts.md` with datasheet section references (the RH850 repo's habit of citing the BIOS script line numbers is the model).
3. **How shared is the peripheral IP really** between MSPM0 and MSPM33? If register maps match, `hal/` needs no backends at all; if not, the backend split adds a layer — decide at Phase 4 with evidence, not now.
4. **BSL flashing of protected/production parts** — check BSL password/lockout behavior (automotive parts often ship with BSL locked after production); the field-update story may need to move to the I2C FW-update page (0xF000) + bootloader earlier than on RH850.
5. **Flash write-protection / option bytes equivalents** (BCR/BSL config on MSPM0) — decide the production configuration early; it affects `bsl_flash.py` and manufacturing flow.

---

## 10. Summary of recommendations

1. **New repo `mspm-baremetal`, separate from RH850** — different toolchain/licensing/CI reality; mirror the structure and conventions, not the code.
2. **One repo for both MSPM0C1106 and MSPM33C321** — same toolchain and model; device differences isolated in `ti/`, `device/`, `make/device_*.mk`, `arch/`.
3. **Open-source everything:** arm-none-eabi-gcc + newlib-nano, GNU Make, OpenOCD/pyOCD, TI ROM BSL via own Python tool for the Pi field flow, cppcheck/clang-tidy/-fanalyzer, Unity host tests, GitHub Actions CI.
4. **Own thin HAL over vendored BSD-licensed TI headers**; apps never touch registers; HAL API kept signature-compatible with the RH850 HAL.
5. **The I2C 16-bit register-map protocol is the shared product contract** across all your MCUs — implement it once as `lib_regmap`, keep the spec doc canonical in both repos.
6. **Bake in the RH850 review lessons as day-one design rules** (§3 table): correct `.data`/`.bss` startup with canaries, watchdog + fault recorder always on, dependency-tracked pinned-flag builds, instanced drivers instead of copies, IRQ-driven debug UART, CI matrix from the first commit.
