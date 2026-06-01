#!/usr/bin/env python3
"""
run_tests.py — Run all FrostVistaOS test cases in QEMU and check results.

Usage (from project root):
  ./scripts/run_tests.py                          # run all tests
  ./scripts/run_tests.py -t sbrk                  # single test
  ./scripts/run_tests.py -b bare -T 20            # bare boot, 20s timeout
  ./scripts/run_tests.py -v                       # verbose (print QEMU output live)
  ./scripts/run_tests.py --list                   # list available tests
  ./scripts/run_tests.py --check logs/            # re-parse existing logs only
  ./scripts/run_tests.py --skip-kernel            # skip kernel rebuild
"""

import argparse
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

# ── colour helpers ──────────────────────────────────────────────────


class Col:
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    YELLOW = '\033[0;33m'
    CYAN = '\033[0;36m'
    DIM = '\033[2m'
    NC = '\033[0m'

# ── test list ───────────────────────────────────────────────────────

TESTS = [
    "sbrk",
    "fork",
    "sys_write",
    "argc",
    "io",
    "vfs",
    "final",
    "lazy_copy",
    "while",
    "init",
    "echo",
    "runner",
]

# ── result pattern ──────────────────────────────────────────────────

RESULT_RE = re.compile(r'^=== (PASS|FAIL):\s*(.+?)\s*===$')
PANIC_RE = re.compile(r'\bpanic\b', re.IGNORECASE)


def check_output(text):
    """Parse unified test markers from QEMU serial output.

    Returns (passed_names, failed_names) – both lists of strings.
    """
    passed = []
    failed = []
    for line in text.splitlines():
        m = RESULT_RE.match(line.strip())
        if not m:
            continue
        status, name = m.group(1), m.group(2)
        if status == 'PASS':
            passed.append(name)
        else:
            failed.append(name)
    return passed, failed


Result = {
    'PASS': 'PASS',
    'FAIL': 'FAIL',
    'TIMEOUT': 'TIMEOUT',
    'BUILD_FAIL': 'BUILD_FAIL',
    'UNCERTAIN': 'UNCERTAIN',
}


# ── subprocess helpers ──────────────────────────────────────────────


def _run(cmd, timeout=None, cwd=None, capture=True, **kwargs):
    """Wrap subprocess.run with common defaults."""
    opts = dict(
        capture_output=capture,
        text=True,
        errors='replace',
        cwd=cwd or PROJ_ROOT,
        preexec_fn=os.setsid,  # isolate process group
    )
    opts.update(kwargs)
    try:
        cp = subprocess.run(cmd, timeout=timeout, **opts)
        return cp.returncode, cp.stdout, cp.stderr
    except subprocess.TimeoutExpired as e:
        return -1, (e.stdout or ''), (e.stderr or '')


def build_kernel(boot, fs_list, rootfs):
    """Run 'make all' to build the kernel and default test."""
    print(f"  Building kernel [{boot}] ... ", end='', flush=True)
    rc, out, err = _run([
        'make', 'all',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
    ])
    if rc != 0:
        print(f'{Col.RED}FAIL{Col.NC}')
        print(err[-1000:] if err else out[-1000:])
        return False
    print(f'{Col.GREEN}OK{Col.NC}')
    return True


def build_test_and_relink(test, boot, fs_list, rootfs):
    """Build test binary, then re-link kernel so it embeds the new init."""
    rc, out, err = _run([
        'make', 'build_test',
        f'TEST={test}',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
    ])
    if rc != 0:
        return False, out, err

    # touch exec.c so make recompiles it with the new init_code.h
    os.utime(str(PROJ_ROOT / 'kernel' / 'core' / 'exec.c'), None)

    rc, out, err = _run([
        'make', 'build/kernel.elf',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
        'BUILD=release',
        f'TEST={test}',
    ])
    return rc == 0, out, err


def run_qemu(test, boot, fs_list, rootfs, timeout, verbose=False):
    """Run QEMU with ``make run`` and return (rc, combined_output).

    Raises TimeoutExpired on timeout.
    """
    cmd = [
        'make', 'run',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
        'BUILD=release',
        f'TEST={test}',
    ]

    cp = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        errors='replace',
        cwd=PROJ_ROOT,
        preexec_fn=os.setsid,
        timeout=timeout,
    )
    return cp.returncode, cp.stdout, cp.stderr


def strip_ansi(s):
    return re.sub(r'\x1b\[[0-9;]*m', '', s)


# ── main logic ──────────────────────────────────────────────────────


def classify(text, test):
    """Return one of PASS / FAIL / TIMEOUT / UNCERTAIN."""
    clean = strip_ansi(text)

    passed, failed = check_output(clean)
    if failed:
        return 'FAIL'
    if passed:
        return 'PASS'

    if PANIC_RE.search(clean):
        return 'FAIL'

    return 'UNCERTAIN'


def run_one_test(test, boot, fs_list, rootfs, timeout, verbose, log_dir):
    """Build + run a single test, return (status, duration_sec, log_path)."""
    start = time.time()

    # 1) build test + relink kernel
    ok, bout, berr = build_test_and_relink(test, boot, fs_list, rootfs)
    if not ok:
        return ('BUILD_FAIL', time.time() - start, None)

    # 2) run in QEMU
    try:
        rc, sout, serr = run_qemu(test, boot, fs_list, rootfs, timeout, verbose)
    except subprocess.TimeoutExpired as e:
        dur = time.time() - start
        partial = (e.stdout or '') + (e.stderr or '')
        # save partial output
        log_path = log_dir / f'{test}_{boot}.log'
        log_path.write_text(strip_ansi(partial))
        # make sure QEMU process group is dead
        if e.stdout and verbose:
            print(strip_ansi(e.stdout[-500:]))
        return ('TIMEOUT', dur, log_path)

    dur = time.time() - start
    combined = (sout or '') + (serr or '')
    status = classify(combined, test)

    # save log
    log_path = log_dir / f'{test}_{boot}.log'
    log_path.write_text(strip_ansi(combined))
    return (status, dur, log_path)


def print_summary(results, total_time):
    """Print results table."""
    print()
    print(f'{Col.CYAN}{"=" * 50}{Col.NC}')
    print(f'{Col.CYAN}  FrostVistaOS Test Results{Col.NC}')
    print(f'{Col.CYAN}{"=" * 50}{Col.NC}')
    print(f'  {"test":<18} {"status":<12} {"duration":<8}  log')
    print(f'  {"─" * 48}')

    counts = {'PASS': 0, 'FAIL': 0, 'TIMEOUT': 0, 'BUILD_FAIL': 0, 'UNCERTAIN': 0}

    for test, status, dur, log_path in results:
        counts[status] = counts.get(status, 0) + 1
        if status == 'PASS':
            sc = Col.GREEN
        elif status == 'FAIL':
            sc = Col.RED
        elif status == 'TIMEOUT':
            sc = Col.YELLOW
        elif status == 'BUILD_FAIL':
            sc = Col.RED
        else:
            sc = Col.YELLOW

        log_tag = f' {log_path.name}' if log_path else ''
        print(f'  {test:<18} {sc}{status:<12}{Col.NC} {dur:<6.1f}s{log_tag}')

    print(f'  {"─" * 48}')
    total = len(results)
    ok = counts.get('PASS', 0)
    fail = counts.get('FAIL', 0) + counts.get('BUILD_FAIL', 0)
    timeout = counts.get('TIMEOUT', 0)
    uncert = counts.get('UNCERTAIN', 0)
    print(f'  {Col.GREEN}{ok} PASS{Col.NC}, '
          f'{Col.RED}{fail} FAIL{Col.NC}, '
          f'{Col.YELLOW}{timeout} TIMEOUT{Col.NC}'
          + (f', {Col.YELLOW}{uncert} UNCERTAIN{Col.NC}' if uncert else ''))
    print(f'  Total time: {total_time:.1f}s')
    print(f'{Col.CYAN}{"=" * 50}{Col.NC}')


def discover_tests():
    """Return all test names from test/test_*.c files."""
    tests = []
    for f in sorted(Path(PROJ_ROOT / 'test').glob('test_*.c')):
        name = f.stem[len('test_'):]
        tests.append(name)
    return tests


# ── entry point ─────────────────────────────────────────────────────


def parse_args(argv=None):
    p = argparse.ArgumentParser(
        description='Run FrostVistaOS tests in QEMU.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument('-t', '--test', help='Run only this test (omit test_ prefix)')
    p.add_argument('-b', '--boot', default='opensbi', choices=['bare', 'opensbi'],
                   help='Boot method (default: opensbi)')
    p.add_argument('-T', '--timeout', type=int, default=15,
                   help='QEMU timeout per test in seconds (default: 15)')
    p.add_argument('-v', '--verbose', action='store_true',
                   help='Print QEMU output live while running')
    p.add_argument('-o', '--log-dir', default='logs',
                   help='Log output directory (default: logs/)')
    p.add_argument('--list', action='store_true',
                   help='List available tests and exit')
    p.add_argument('--check', metavar='DIR',
                   help='Re-parse log files from DIR instead of running QEMU')
    p.add_argument('--skip-kernel', action='store_true',
                   help='Skip the initial kernel build')
    return p.parse_args(argv)


def main():
    args = parse_args()

    global PROJ_ROOT
    PROJ_ROOT = Path(__file__).resolve().parent.parent

    if args.list:
        print('Available tests:')
        for t in discover_tests():
            print(f'  {t}')
        return 0

    # determine test list
    if args.test:
        test_list = [args.test]
    else:
        test_list = TESTS[:]

    if args.check:
        # re-parse mode
        log_dir = Path(args.check)
        if not log_dir.is_dir():
            print(f'{Col.RED}Error: {args.check} is not a directory{Col.NC}')
            return 1
        results = []
        for f in sorted(log_dir.glob('*.log')):
            text = f.read_text(errors='replace')
            test_name = f.stem.split('_')[0]  # crude, but works
            status = classify(text, test_name)
            results.append((test_name, status, 0.0, f))
        print_summary(results, 0.0)
        return 0 if all(s == 'PASS' for _, s, _, _ in results) else 1

    # ensure log dir
    log_dir = Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)

    boot = args.boot
    fs_list = 'ext4 devtmpfs'
    rootfs = 'ext4'
    timeout = args.timeout
    verbose = args.verbose

    print(f'{Col.CYAN}FrostVistaOS Test Runner{Col.NC}')
    print(f'  Tests: {len(test_list)}   Boot: {boot}   Timeout: {timeout}s')
    print()

    # ── build kernel ────────────────────────────────────────────────
    if not args.skip_kernel:
        if not build_kernel(boot, fs_list, rootfs):
            print(f'{Col.RED}Kernel build failed, aborting.{Col.NC}')
            return 1
        print()

    # ── run tests ───────────────────────────────────────────────────
    results = []
    start_total = time.time()

    try:
        for idx, test in enumerate(test_list, 1):
            print(f'  [{idx}/{len(test_list)}] {test:<18} ', end='', flush=True)

            status, dur, log_path = run_one_test(
                test, boot, fs_list, rootfs, timeout, verbose, log_dir,
            )

            if status == 'PASS':
                sc = f'{Col.GREEN}PASS{Col.NC}'
            elif status == 'FAIL':
                sc = f'{Col.RED}FAIL{Col.NC}'
            elif status == 'TIMEOUT':
                sc = f'{Col.YELLOW}TIMEOUT{Col.NC}'
            elif status == 'BUILD_FAIL':
                sc = f'{Col.RED}BUILD_FAIL{Col.NC}'
            else:
                sc = f'{Col.YELLOW}UNCERTAIN{Col.NC}'

            print(f'{sc}  ({dur:.1f}s)')
            results.append((test, status, dur, log_path))

    except KeyboardInterrupt:
        print(f'\n{Col.YELLOW}Interrupted, printing partial results.{Col.NC}')

    total_time = time.time() - start_total
    print_summary(results, total_time)

    any_fail = any(s in ('FAIL', 'BUILD_FAIL') for _, s, _, _ in results)
    return 1 if any_fail else 0


if __name__ == '__main__':
    sys.exit(main())
