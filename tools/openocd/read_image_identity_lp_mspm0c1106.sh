#!/usr/bin/env sh
# Read the fixed identity block from the target and compare it with an ELF.

set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <canonical.elf>" >&2
    exit 64
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
openocd_bin=${OPENOCD:-openocd}
python_bin=${PYTHON:-python3}
readback=$(mktemp "${TMPDIR:-/tmp}/mspm-image-identity.XXXXXX")

cleanup() {
    rm -f "$readback"
}
trap cleanup EXIT HUP INT TERM

if ! command -v "$openocd_bin" >/dev/null 2>&1; then
    echo "OpenOCD executable not found: $openocd_bin" >&2
    exit 127
fi

"$openocd_bin" -f "$script_dir/lp_mspm0c1106.cfg" \
    -c "init; halt; dump_image $readback 0x0000ffc0 0x00000040; resume; shutdown"

"$python_bin" "$project_root/tools/stamp_image_identity.py" verify-readback \
    --elf "$1" --readback "$readback" --flash-origin 0x00000000 --flash-length 0x00010000
