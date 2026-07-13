# Project context

This is a C11, GNU Make, bare-metal platform for TI MSPM0 and MSPM33 MCUs.
The current implementation is Phase 0 and supports a link-complete
`lp_mspm0c1106` / `blink` placeholder only; it is not ready to flash.

## Commands

```sh
make BOARD=lp_mspm0c1106 APP=blink
make BOARD=lp_mspm0c1106 APP=blink DEBUG=on VERSION=01.02
make test
make format-check
```

## Non-negotiable rules

- Apps never include `ti/` headers or access registers directly.
- Keep `lib/` host-testable and register-free.
- Do not add a normal command that erases or writes MSPM0C1106 BCR/BSL
  configuration NVM.
- Do not invent MSPM33 facts, TrustZone policy, a bootloader, or a field update
  protocol before their plan gates are met.
- Update `docs/device_facts.md` before introducing a hardware-dependent
  constant or workflow.
