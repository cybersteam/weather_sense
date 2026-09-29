#!/usr/bin/env python3
"""Fail the AVR build when the image no longer fits an Uno with Optiboot.

Berkeley ``avr-size`` on Binutils 2.46 reports ``.data`` inside the text
column and a data column of zero, because the linker marks the section
readonly. Sysv output (``-A``) still lists each section, so the gate sums
those sizes itself.

Flash is every allocated image section that is not RAM: ``.text`` plus the
flash-resident initializer for ``.data``. SRAM is ``.data`` + ``.bss`` +
``.noinit``. Debug, comment, and device-info notes are ignored.
"""

import os
import subprocess
import sys

FLASH_LIMIT = 30720  # 32 KiB part, 512-byte Optiboot, with margin
SRAM_LIMIT = 1536  # 2 KiB SRAM, leaving room for the stack
RAM_VMA = 0x800000  # avr-gcc data address space


def section_rows(text):
    rows = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue
        name, size, addr = parts
        if not name.startswith("."):
            continue
        rows.append((name, int(size), int(addr)))
    return rows


def measure(rows):
    flash = 0
    sram = 0
    for name, size, addr in rows:
        if name.startswith(".debug") or name.startswith(".note") or name == ".comment":
            continue
        if name.startswith(".eeprom"):
            continue
        if addr >= RAM_VMA:
            sram += size
            if name == ".data" or name.startswith(".data."):
                flash += size
            continue
        flash += size
    return flash, sram


def main():
    if len(sys.argv) != 2:
        print("usage: size_gate.py firmware.elf", file=sys.stderr)
        return 2
    elf = sys.argv[1]
    size = os.environ.get("SIZE_CMD", "avr-size")
    out = subprocess.check_output([size, "-A", elf], text=True)
    rows = section_rows(out)
    if not rows:
        print("size_gate: no sections from avr-size -A", file=sys.stderr)
        return 2
    flash, sram = measure(rows)
    print(f"flash_bytes {flash} (limit {FLASH_LIMIT})")
    print(f"sram_bytes {sram} (limit {SRAM_LIMIT})")
    failed = False
    if flash > FLASH_LIMIT:
        print(f"flash {flash} exceeds {FLASH_LIMIT}", file=sys.stderr)
        failed = True
    if sram > SRAM_LIMIT:
        print(f"sram {sram} exceeds {SRAM_LIMIT}", file=sys.stderr)
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
