#!/bin/sh
# extract-kernel-symbols.sh — turn YOUR OWN X9 firmware dump's KL partition
# into a fully-symbolized vmlinux (kallsyms), for offline analysis.
#
#   ./extract-kernel-symbols.sh /path/to/KL.BIN [output.vmlinux]
#
# Path: uImage header (64 B) -> uncompressed ARM zImage -> LZ4 piggy ->
# raw vmlinux -> ELF with reconstructed kallsyms (vmlinux-to-elf).
set -e
IN=${1:?usage: extract-kernel-symbols.sh KL.BIN [out.vmlinux]}
OUT=${2:-kl.vmlinux}
HERE=$(cd "$(dirname "$0")" && pwd)

command -v lz4 >/dev/null || { echo "need lz4 (pacman -S lz4 / apt install lz4)"; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# 1. strip the u-boot uImage header
dd if="$IN" of="$WORK/zImage" bs=64 skip=1 status=none

# 2. locate the LZ4 piggy (kernel >= 4.15 zImages) and decompress
OFF=$(python3 - "$WORK/zImage" <<'EOF'
import sys
d = open(sys.argv[1], 'rb').read()
off = d.find(b'\x02\x21\x4c\x18')          # lz4 legacy frame magic
print(off if off >= 0 else -1)
EOF
)
[ "$OFF" -ge 0 ] || { echo "no LZ4 payload found in zImage"; exit 1; }
dd if="$WORK/zImage" of="$WORK/piggy.lz4" bs=1 skip="$OFF" status=none
lz4 -d "$WORK/piggy.lz4" "$WORK/vmlinux.bin" 2>/dev/null || \
  lz4 -d --no-sparse "$WORK/piggy.lz4" "$WORK/vmlinux.bin"
ls -la "$WORK/vmlinux.bin"

# 3. reconstruct the ELF with kallsyms symbols (base = load address from the
#    uImage header: physical 0x20208000 -> virtual 0xC0208000, PAGE_OFFSET
#    0xC0000000, no KASLR on ARM32)
docker run --rm -v "$WORK":/w -v "$HERE":/t:ro python:3.11 \
  sh -c 'pip install -q vmlinux-to-elf && vmlinux-to-elf /w/vmlinux.bin /w/vmlinux --base 0xC0208000'
cp "$WORK/vmlinux" "$OUT"
echo "== $OUT =="
docker run --rm -v "$PWD":/w -w /w debian:bookworm-slim sh -c \
  'apt-get -qq update >/dev/null && apt-get -qq install -y binutils-arm-linux-gnueabihf >/dev/null 2>&1 && arm-linux-gnueabihf-nm "'"$(basename "$OUT")"'" | head -5 && echo ... $(arm-linux-gnueabihf-nm "'"$(basename "$OUT")"'" | wc -l) symbols'
