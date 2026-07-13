#!/usr/bin/env sh
# Read-only OpenOCD/XDS110 smoke check for the LP-MSPM0C1106.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
openocd_bin=${OPENOCD:-openocd}

if ! command -v "$openocd_bin" >/dev/null 2>&1; then
    echo "OpenOCD executable not found: $openocd_bin" >&2
    echo "Set OPENOCD to a current upstream build with XDS110 and MSPM0 support." >&2
    exit 127
fi

exec "$openocd_bin" -f "$script_dir/lp_mspm0c1106.cfg" \
    -c "init; halt; flash info 0; shutdown"
