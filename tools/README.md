# Hardware tools

The first MSPM0C1106 hardware check is deliberately read-only:

```sh
OPENOCD=/path/to/openocd tools/openocd/probe_lp_mspm0c1106.sh
```

Use an upstream OpenOCD build with the XDS110 adapter and MSPM0 flash driver
enabled. The initial test candidate is upstream commit
`cb52502e88832386610add9781030f5380344063` (checked 2026-07-13); it is not
considered known-good until this probe succeeds on the reference board. A
typical source build is:

```sh
git clone https://github.com/openocd-org/openocd.git
cd openocd
git checkout cb52502e88832386610add9781030f5380344063
./bootstrap
./configure --enable-xds110
make -j"$(nproc)"
```

The configuration declares **only MAIN flash** at `0x00000000`; it contains no
NONMAIN, BCR, BSL, data-flash, mass-erase, or factory-reset command.

Do not use an unreviewed OpenOCD target configuration or a `factory_reset`
command.  The repository will add `make flash` and `make gdb` only after this
probe check and a MAIN-only program/readback sequence have been recorded on
the reference board.
