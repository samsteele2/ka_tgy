#!/usr/bin/env bash
# Flash a reviewed KA nFET firmware image on macOS/Linux using a USBasp.
# Writes the board-specific low and high fuse bytes; lock bits are unchanged.

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
hex_path="$repo_dir/ka_nfet.hex"

if ! command -v avrdude >/dev/null 2>&1; then
    echo "AVRDUDE was not found. Install it first (for macOS: brew install avrdude)." >&2
    exit 1
fi

if [[ ! -f "$hex_path" ]]; then
    echo "Firmware image not found: $hex_path" >&2
    exit 1
fi

echo "Writing flash, low fuse 0x3F, and high fuse 0xCA."
echo "The lock byte is unchanged."
avrdude -c usbasp -p m8 \
    -U "flash:w:${hex_path}:i" \
    -U "lfuse:w:0x3F:m" \
    -U "hfuse:w:0xCA:m"

echo "Verifying flash and fuse bytes..."
avrdude -c usbasp -p m8 \
    -U "flash:v:${hex_path}:i" \
    -U "lfuse:v:0x3F:m" \
    -U "hfuse:v:0xCA:m"

echo "Flash and verification completed successfully."
