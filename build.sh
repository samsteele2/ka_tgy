#!/usr/bin/env bash
# Build the KA nFET firmware on macOS/Linux using the KA-only Makefile.

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v avra >/dev/null 2>&1; then
    echo "AVRA was not found. Install it first (for macOS: brew install avra)." >&2
    exit 1
fi

if ! command -v make >/dev/null 2>&1; then
    echo "make was not found. Install the macOS Command Line Tools, then run this script again." >&2
    exit 1
fi

exec make -C "$repo_dir" "AVRA=$(command -v avra)"
