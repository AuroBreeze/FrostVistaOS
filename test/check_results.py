#!/usr/bin/env python3
"""
check_results.py — Parse QEMU serial output for unified test results.

Usage:
  make qemu BOOT=opensbi ROOTFS=ext4 TEST=runner 2>&1 | tee qemu.log
  python3 test/check_results.py qemu.log

  # Or piped directly:
  make qemu ... 2>&1 | python3 test/check_results.py

Output format expected:
  === TEST <name> ===
  === PASS: <name> ===
  === FAIL: <name> ===
"""

import sys
import re

RESULT_RE = re.compile(r'^=== (PASS|FAIL):\s*(.+?)\s*===$')


def check_results(lines):
    passed = []
    failed = []

    for line in lines:
        m = RESULT_RE.match(line)
        if not m:
            continue
        status, name = m.group(1), m.group(2)
        if status == 'PASS':
            passed.append(name)
        else:
            failed.append(name)

    return passed, failed


def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            passed, failed = check_results(f)
    else:
        passed, failed = check_results(sys.stdin)

    total = len(passed) + len(failed)
    print()
    print(f"{'=' * 50}")
    print(f"  Results: {total} test(s)")
    print(f"{'=' * 50}")
    for name in passed:
        print(f"  PASS: {name}")
    for name in failed:
        print(f"  FAIL: {name}")
    print(f"{'=' * 50}")
    if failed:
        print(f"  {len(failed)}/{total} FAILED")
    else:
        print(f"  All {total} passed!")
    print(f"{'=' * 50}")

    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
