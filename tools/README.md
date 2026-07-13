# Hardware tools

The LP-MSPM0C1106 probe and MAIN-only program/verify path was bench-validated
on 2026-07-13. The evidence is in
[`docs/bringup_lp_mspm0c1106.md`](../docs/bringup_lp_mspm0c1106.md).

The read-only hardware check remains useful before programming:

```sh
OPENOCD=/path/to/openocd tools/openocd/probe_lp_mspm0c1106.sh
```

Use an upstream OpenOCD build with the XDS110 adapter and MSPM0 flash driver
enabled. The known-good source revision is upstream commit
`cb52502e88832386610add9781030f5380344063`; its local version string was
`OpenOCD 0.12.0+dev-gcb52502`. A typical source build is:

```sh
git clone https://github.com/openocd-org/openocd.git
cd openocd
git checkout cb52502e88832386610add9781030f5380344063
git submodule update --init --recursive
./bootstrap
CCACHE_DISABLE=1 ./configure --enable-cmsis-dap --enable-internal-jimtcl --disable-werror
CCACHE_DISABLE=1 make -j1
```

Program and verify the ELF with the board-specific, MAIN-only configuration:

```sh
make OPENOCD=/path/to/openocd BOARD=lp_mspm0c1106 APP=blink flash
```

After flashing, compare the target's read-only image-identity block with the
canonical ELF. This halts and resumes the core but never erases or programs
flash:

```sh
make OPENOCD=/path/to/openocd BOARD=lp_mspm0c1106 APP=blink \
  DEBUG=off identity-readback
```

See [`docs/image_identity.md`](../docs/image_identity.md) for the byte format
and CRC coverage rule.

The configuration declares **only MAIN flash** at `0x00000000`; it contains no
NONMAIN, BCR, BSL, data-flash, mass-erase, or factory-reset command. The
`program ... verify reset exit` command can erase only the MAIN-flash sectors
needed for the supplied ELF.

Start a GDB server without programming the target:

```sh
make OPENOCD=/path/to/openocd BOARD=lp_mspm0c1106 APP=blink gdb
# In another terminal:
arm-none-eabi-gdb output/lp_mspm0c1106/blink/release/lp_mspm0c1106_blink.elf \
  -ex 'target extended-remote :3333'
```

On native Linux, install [`99-xds110.rules`](99-xds110.rules) once, add the
developer account to `plugdev`, then replug the board or reload udev rules:

```sh
sudo install -m 0644 tools/99-xds110.rules /etc/udev/rules.d/99-xds110.rules
sudo usermod -aG plugdev "$USER"
sudo udevadm control --reload-rules
```

On WSL2, udev may not manage USBIP-attached devices. A USB-attached XDS110 can
therefore appear as a root-owned device node. For a temporary single-session
workaround, grant the current user read/write access to that node after every
board reattachment:

```sh
sudo chmod a+rw /dev/bus/usb/001/002
```

Confirm the current bus/device number with `lsusb` before using this command.

Do not use an unreviewed OpenOCD target configuration or a `factory_reset`
command.
