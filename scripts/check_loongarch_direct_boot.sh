#!/bin/sh
set -eu

elf=${1:?usage: check_loongarch_direct_boot.sh <kernel.elf> [readelf]}
readelf_bin=${2:-loongarch64-unknown-linux-gnu-readelf}

header=$($readelf_bin -h "$elf")
segments=$($readelf_bin -lW "$elf")

printf '%s\n' "$header" | grep -Eq \
    'Entry point address:[[:space:]]+0x8000000000200000' || {
	printf '%s\n' 'unexpected LoongArch kernel entry' >&2
	exit 1
}

printf '%s\n' "$segments" | grep -Eq \
    'LOAD[[:space:]]+0x[0-9a-f]+[[:space:]]+0x8000000000200000[[:space:]]+0x0000000000200000' || {
	printf '%s\n' 'missing LoongArch DMW0 text load segment' >&2
	exit 1
}

if printf '%s\n' "$segments" | grep -Eq \
    'LOAD[[:space:]]+0x[0-9a-f]+[[:space:]]+0x[0-9a-f]+[[:space:]]+0x00000000000[0-9a-f]{5}'; then
	printf '%s\n' 'LoongArch kernel overlaps QEMU boot info below 1 MiB' >&2
	exit 1
fi
