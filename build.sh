#!/usr/bin/env bash
# Build the KA nFET firmware on macOS/Linux. Requires AVRA 1.3.x in PATH.

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v avra >/dev/null 2>&1; then
    echo "AVRA was not found. Install it first (for macOS: brew install avra)." >&2
    exit 1
fi

(
    cd "$repo_dir"
    avra -fI -D ka_nfet_esc -I "$repo_dir" -I "$repo_dir/other_escs" tgy.asm
    mv -f tgy.hex ka_nfet.hex
    rm -f tgy.eep.hex tgy.obj tgy.cof
)

echo "Built: $repo_dir/ka_nfet.hex"