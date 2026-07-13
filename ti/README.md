# Vendored headers

This directory contains only the register-definition headers required to build
the MSPM0C1106 target.  Application code must not include these files directly.

## Provenance

- **TI MSPM0 SDK:** `mspm0_sdk_2_10_00_04`, commit
  `e249e2bd63bcc912176a30a45a6a5dcea951168b`, from TI's public
  `TexasInstruments/mspm0-sdk` repository.
- **TI device headers:** `mspm0c1105_c1106.h`, `DeviceFamily.h`, and the
  peripheral headers they include.  Their source-file notices are BSD-3-Clause;
  see `LICENSES/TI-BSD-3-Clause.txt`.
- **CMSIS Core:** CMSIS 5.0.9 subset for Cortex-M0+ (`core_cm0plus.h` and its
  direct dependencies).  It is Apache-2.0; see `LICENSES/Apache-2.0.txt`.

The SDK's top-level commercial licence is not copied here.  Before updating
this snapshot, review the upstream manifest and every changed file's header.
Retain upstream copyright notices and update this provenance record with the
new tag and immutable commit.

