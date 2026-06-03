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
    NC = '\033[0m'

# ── test list ───────────────────────────────────────────────────────

TESTS = [
    "sbrk",
    "fork",
    "sys_write",
    "sys_misc",
    "sys_pipe",
    "open",
    "easyfs_maxfile",
    "easyfs_unlink",
    "easyfs_mkdir",
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

EASYFS_TESTS = {
    "open",
    "easyfs_maxfile",
    "easyfs_unlink",
    "easyfs_mkdir",
}

# ── result pattern ──────────────────────────────────────────────────

RESULT_RE = re.compile(r'^=== (PASS|FAIL):\s*(.+?)\s*===$')
PANIC_RE = re.compile(r'\bpanic\b', re.IGNORECASE)
LOG_ERROR_RE = re.compile(r'\[\s*ERROR\s*\]')
LOG_WARN_RE = re.compile(r'\[\s*WARN\s*\]')
DIAG_RE = re.compile(r'\[\s*(ERROR|WARN)\s*\]')

EXPECTED_DIAGNOSTICS = {
    # Negative syscall tests intentionally pass bad user pointers/fds.  These
    # logs are expected only for the named test; any new WARN/ERROR still fails.
    'sys_write': [
        r'Access Violation: va 0x0+ is in unmapped space',
        r'copyin: pte not valid or lack permissions',
        r'sys_write: copyin failed',
    ],
    'sys_misc': [
        r'copyout: pte not valid or lack permissions',
        r'sys_dup3: oldfd=-1 is not valid',
        r'sys_dup3: newfd=\d+ is the same as oldfd=\d+',
        r'sys_dup3: flags=1 is not supported',
    ],
    'sys_pipe': [
        r'Access Violation: va 0x0+ is in unmapped space',
        r'pipe2: flags not supported',
        r'copyin: pte not valid or lack permissions',
        r'copyout: pte not valid or lack permissions',
        r'copyout failed',
        r'sys_read: file \d+ not readable',
        r'sys_write: file \d+ not writable',
    ],
    'io': [
        r'sys_write: file \d+ not open',
        r'sys_read: file \d+ not open',
    ],
    'vfs': [
        r'sys_write: file \d+ not writable',
    ],
}

EXPECTED_DIAGNOSTIC_COUNTS = {
    'sys_pipe': {
        r'Access Violation: va 0x0+ is in unmapped space': 1,
        r'pipe2: flags not supported': 1,
        r'copyin: pte not valid or lack permissions': 1,
        r'copyout: pte not valid or lack permissions': 2,
        r'copyout failed': 1,
        r'sys_read: file \d+ not readable': 1,
        r'sys_write: file \d+ not writable': 1,
    },
}


def check_output(text):
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


def diagnostic_lines(text):
    return [line.strip() for line in text.splitlines() if DIAG_RE.search(line)]


def unexpected_diagnostics(text, test):
    expected = EXPECTED_DIAGNOSTICS.get(test, [])
    expected_counts = EXPECTED_DIAGNOSTIC_COUNTS.get(test, {})
    seen_counts = {pattern: 0 for pattern in expected_counts}
    unexpected = []
    for line in diagnostic_lines(text):
        matched = None
        for pattern in expected:
            if re.search(pattern, line):
                matched = pattern
                break
        if matched is None:
            unexpected.append(line)
            continue
        if matched in expected_counts:
            seen_counts[matched] += 1
            if seen_counts[matched] > expected_counts[matched]:
                unexpected.append(line)
    return unexpected


# ── helpers ──────────────────────────────────────────────────────────


def _to_str(s):
    """Convert None/bytes/str to str."""
    if s is None:
        return ''
    if isinstance(s, bytes):
        return s.decode('utf-8', errors='replace')
    return str(s)


def strip_ansi(s):
    s = _to_str(s)
    return re.sub(r'\x1b\[[0-9;]*m', '', s)


def kill_stale_qemu():
    """Kill any leftover QEMU processes to release the disk image lock."""
    try:
        subprocess.run(['pkill', '-9', '-f', 'qemu-system-riscv'],
                       capture_output=True, timeout=5)
    except Exception:
        pass
    time.sleep(0.3)  # let the kernel release the file lock


def _make(*args, cwd=None, timeout=None):
    """Run a make command, return (rc, stdout, stderr)."""
    cmd = ['make', *args]
    try:
        cp = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            errors='replace',
            cwd=cwd or PROJ_ROOT,
            timeout=timeout,
        )
        return cp.returncode, cp.stdout, cp.stderr
    except subprocess.TimeoutExpired as e:
        return -1, (e.stdout or ''), (e.stderr or '')


def classify(text, test):
    """Return PASS / PASS_EXPECTED_LOG / PASS_WARN / PASS_ERROR / FAIL / UNCERTAIN."""
    clean = strip_ansi(text)

    passed, failed = check_output(clean)
    unexpected = unexpected_diagnostics(clean, test)
    if failed:
        return 'FAIL'
    if PANIC_RE.search(clean):
        return 'FAIL'
    if passed:
        if not unexpected and diagnostic_lines(clean):
            return 'PASS_EXPECTED_LOG'
        if any(LOG_ERROR_RE.search(line) for line in unexpected):
            return 'PASS_ERROR'
        if any(LOG_WARN_RE.search(line) for line in unexpected):
            return 'PASS_WARN'
        return 'PASS'

    if any(LOG_ERROR_RE.search(line) for line in unexpected):
        return 'FAIL'
    if any(LOG_WARN_RE.search(line) for line in unexpected):
        return 'UNCERTAIN_WARN'

    return 'UNCERTAIN'


def log_test_name(path):
    """Recover test name from <test>_<boot>.log."""
    stem = path.stem
    for boot in ('opensbi', 'bare'):
        suffix = f'_{boot}'
        if stem.endswith(suffix):
            return stem[:-len(suffix)]
    return stem


# ── build steps ──────────────────────────────────────────────────────


def build_kernel(boot, fs_list, rootfs, log_level):
    """make all — build kernel once with the default test embedded."""
    print(f"  Building kernel [{boot}] ... ", end='', flush=True)
    rc, out, err = _make(
        'all',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
        f'LOG={log_level}',
    )
    if rc != 0:
        print(f'{Col.RED}FAIL{Col.NC}')
        tail = (err + out)[-800:]
        print(tail)
        return False
    print(f'{Col.GREEN}OK{Col.NC}')
    return True


def build_test_and_relink(test, boot, fs_list, rootfs, log_level):
    """Build test binary, then rebuild kernel.elf with the selected config."""
    rc, out, err = _make(
        'build_test',
        f'TEST={test}',
    )
    if rc != 0:
        return False, out, err

    # force recompilation of exec.c (which #includes init_code.h)
    os.utime(str(PROJ_ROOT / 'kernel' / 'core' / 'exec.c'), None)

    rc, out, err = _make(
        '-B',
        'build/kernel.elf',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
        'BUILD=release',
        f'LOG={log_level}',
    )
    return rc == 0, out, err


# ── QEMU execution ──────────────────────────────────────────────────


def run_qemu(boot, fs_list, rootfs, log_level, timeout, verbose=False):
    """Spawn QEMU via 'make run'.  Returns (rc, stdout, stderr) or raises TimeoutExpired."""
    cmd = [
        'make', 'run',
        f'BOOT={boot}',
        f'FS_LIST={fs_list}',
        f'ROOTFS={rootfs}',
        'BUILD=release',
        f'LOG={log_level}',
    ]
    stdout_arg = None if verbose else subprocess.PIPE
    stderr_arg = None if verbose else subprocess.PIPE

    try:
        cp = subprocess.run(
            cmd,
            stdout=stdout_arg,
            stderr=stderr_arg,
            text=True,
            errors='replace',
            cwd=PROJ_ROOT,
            timeout=timeout,
        )
        out = (cp.stdout or '')
        err = (cp.stderr or '')
        return cp.returncode, out, err
    except subprocess.TimeoutExpired as e:
        # QEMU didn't exit — kill every remaining instance
        kill_stale_qemu()
        raise


# ── test runner ──────────────────────────────────────────────────────


def run_one_test(test, boot, fs_list, rootfs, log_level, timeout, verbose, log_dir):
    """Build + run a single test. Returns (status, duration, log_path)."""
    start = time.time()

    ok, bout, berr = build_test_and_relink(test, boot, fs_list, rootfs, log_level)
    if not ok:
        dur = time.time() - start
        log_path = log_dir / f'{test}_{boot}.log'
        log_path.write_text(strip_ansi(_to_str(bout) + _to_str(berr)))
        return ('BUILD_FAIL', dur, log_path)

    combined = ''
    try:
        rc, sout, serr = run_qemu(
            boot, fs_list, rootfs, log_level, timeout, verbose,
        )
        combined = _to_str(sout) + _to_str(serr)
        dur = time.time() - start

        status = classify(combined, test)

        # quick failure: QEMU couldn't even start (disk lock etc.).  Test
        # markers win over QEMU's shutdown exit status.
        if status == 'UNCERTAIN' and rc != 0 and dur < 2:
            log_path = log_dir / f'{test}_{boot}.log'
            log_path.write_text(strip_ansi(combined))
            return ('LAUNCH_FAIL', dur, log_path)
    except subprocess.TimeoutExpired as e:
        dur = time.time() - start
        combined = _to_str(getattr(e, 'stdout', None))
        combined += _to_str(getattr(e, 'stderr', None))
        if not combined:
            combined = _to_str(getattr(e, 'output', None))
        status = classify(combined, test)
        if status == 'UNCERTAIN':
            status = 'TIMEOUT'

    log_path = log_dir / f'{test}_{boot}.log'
    log_path.write_text(strip_ansi(combined))
    return (status, dur, log_path)


# ── summary ──────────────────────────────────────────────────────────


def print_summary(results, total_time):
    print()
    header = f'  {"test":<18} {"status":<14} {"duration":<8}  log'
    sep = f'  {"─" * 52}'

    print(f'{Col.CYAN}{"=" * 50}{Col.NC}')
    print(f'{Col.CYAN}  FrostVistaOS Test Results{Col.NC}')
    print(f'{Col.CYAN}{"=" * 50}{Col.NC}')
    print(header)
    print(sep)

    counts = {}
    colour = {
        'PASS': Col.GREEN,
        'PASS_EXPECTED_LOG': Col.CYAN,
        'PASS_WARN': Col.YELLOW,
        'PASS_ERROR': Col.RED,
        'FAIL': Col.RED,
        'TIMEOUT': Col.YELLOW,
        'BUILD_FAIL': Col.RED,
        'LAUNCH_FAIL': Col.RED,
        'UNCERTAIN': Col.YELLOW,
        'UNCERTAIN_WARN': Col.YELLOW,
    }

    for test, status, dur, log_path in results:
        counts[status] = counts.get(status, 0) + 1
        sc = colour.get(status, Col.YELLOW)
        log_tag = f' {log_path.name}' if log_path else ''
        print(f'  {test:<18} {sc}{status:<14}{Col.NC} {dur:<6.1f}s{log_tag}')

    print(sep)

    passes = counts.get('PASS', 0)
    pass_expected_logs = counts.get('PASS_EXPECTED_LOG', 0)
    pass_warns = counts.get('PASS_WARN', 0)
    pass_errors = counts.get('PASS_ERROR', 0)
    fails = counts.get('FAIL', 0)
    timeouts = counts.get('TIMEOUT', 0)
    builds = counts.get('BUILD_FAIL', 0)
    launches = counts.get('LAUNCH_FAIL', 0)
    uncerts = counts.get('UNCERTAIN', 0)
    uncert_warns = counts.get('UNCERTAIN_WARN', 0)

    parts = [f'{Col.GREEN}{passes} PASS{Col.NC}']
    if pass_expected_logs:
        parts.append(f'{Col.CYAN}{pass_expected_logs} PASS_EXPECTED_LOG{Col.NC}')
    if pass_warns:
        parts.append(f'{Col.YELLOW}{pass_warns} PASS_WARN{Col.NC}')
    if pass_errors:
        parts.append(f'{Col.RED}{pass_errors} PASS_ERROR{Col.NC}')
    if fails:
        parts.append(f'{Col.RED}{fails} FAIL{Col.NC}')
    if timeouts:
        parts.append(f'{Col.YELLOW}{timeouts} TIMEOUT{Col.NC}')
    if launches:
        parts.append(f'{Col.RED}{launches} LAUNCH_FAIL{Col.NC}')
    if builds:
        parts.append(f'{Col.RED}{builds} BUILD_FAIL{Col.NC}')
    if uncerts:
        parts.append(f'{Col.YELLOW}{uncerts} UNCERTAIN{Col.NC}')
    if uncert_warns:
        parts.append(f'{Col.YELLOW}{uncert_warns} UNCERTAIN_WARN{Col.NC}')

    print(f'  {", ".join(parts)}')
    print(f'  Total time: {total_time:.1f}s')
    print(f'{Col.CYAN}{"=" * 50}{Col.NC}')


# ── entry point ──────────────────────────────────────────────────────


def parse_args(argv=None):
    p = argparse.ArgumentParser(description='Run FrostVistaOS tests in QEMU.')
    p.add_argument('-t', '--test', help='Run only this test')
    p.add_argument('-b', '--boot', default='opensbi', choices=['bare', 'opensbi'])
    p.add_argument('-T', '--timeout', type=int, default=15,
                   help='QEMU timeout per test in seconds (default: 15)')
    p.add_argument('-v', '--verbose', action='store_true',
                   help='Print QEMU output live (forces sequential, no capture)')
    p.add_argument('-o', '--log-dir', default='logs',
                   help='Log output directory (default: logs/)')
    p.add_argument('--list', action='store_true',
                   help='List available tests and exit')
    p.add_argument('--check', metavar='DIR',
                   help='Re-parse log files from DIR instead of running QEMU')
    p.add_argument('--skip-kernel', action='store_true',
                   help='Skip the initial kernel build')
    p.add_argument('--rootfs', choices=['ext4', 'easyfs'],
                   help='Root filesystem to boot (default: ext4, or easyfs for writable FS tests)')
    p.add_argument('--fs-list',
                   help='Filesystem list passed to make (default follows --rootfs)')
    p.add_argument('--log-level', default='INFO',
                   choices=['TRACE', 'DEBUG', 'INFO', 'WARN', 'ERROR'],
                   help='Kernel log level passed to make LOG=... (default: INFO)')
    return p.parse_args(argv)


def main():
    args = parse_args()

    global PROJ_ROOT
    PROJ_ROOT = Path(__file__).resolve().parent.parent

    if args.list:
        print('Available tests:')
        for f in sorted((PROJ_ROOT / 'test').glob('test_*.c')):
            print(f'  {f.stem[len("test_"):]}')
        return 0

    test_list = [args.test] if args.test else TESTS[:]

    if args.check:
        log_dir = Path(args.check)
        if not log_dir.is_dir():
            print(f'{Col.RED}Error: {args.check} is not a directory{Col.NC}')
            return 1
        results = []
        for f in sorted(log_dir.glob('*.log')):
            text = f.read_text(errors='replace')
            name = log_test_name(f)
            results.append((name, classify(text, name), 0.0, f))
        print_summary(results, 0.0)
        ok_statuses = ('PASS', 'PASS_EXPECTED_LOG')
        return 0 if all(s in ok_statuses for _, s, _, _ in results) else 1

    boot = args.boot
    needs_easyfs = any(test in EASYFS_TESTS for test in test_list)
    rootfs = args.rootfs or ('easyfs' if needs_easyfs else 'ext4')
    fs_list = args.fs_list or (
        'easyfs devtmpfs' if rootfs == 'easyfs' else 'ext4 devtmpfs'
    )
    vflag = args.verbose
    log_level = args.log_level

    log_dir = Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)

    print(f'{Col.CYAN}FrostVistaOS Test Runner{Col.NC}')
    print(f'  Tests: {len(test_list)}   Boot: {boot}   RootFS: {rootfs}   Log: {log_level}   Timeout: {args.timeout}s')
    print(f'  FS_LIST: {fs_list}')
    print()

    # clean up old QEMU
    kill_stale_qemu()

    if not args.skip_kernel:
        if not build_kernel(boot, fs_list, rootfs, log_level):
            return 1
        print()

    results = []
    start_total = time.time()

    try:
        for idx, test in enumerate(test_list, 1):
            print(f'  [{idx}/{len(test_list)}] {test:<18} ', end='', flush=True)

            status, dur, _ = run_one_test(
                test, boot, fs_list, rootfs, log_level, args.timeout, vflag, log_dir,
            )

            c = {'PASS': Col.GREEN, 'PASS_EXPECTED_LOG': Col.CYAN,
                 'PASS_WARN': Col.YELLOW, 'PASS_ERROR': Col.RED, 'FAIL': Col.RED,
                 'TIMEOUT': Col.YELLOW, 'BUILD_FAIL': Col.RED,
                 'LAUNCH_FAIL': Col.RED, 'UNCERTAIN': Col.YELLOW,
                 'UNCERTAIN_WARN': Col.YELLOW}.get(status, Col.YELLOW)
            print(f'{c}{status}{Col.NC}  ({dur:.1f}s)')
            results.append((test, status, dur, None))

    except KeyboardInterrupt:
        print(f'\n{Col.YELLOW}Interrupted.{Col.NC}')

    total_time = time.time() - start_total
    print_summary(results, total_time)

    any_bad = any(s in ('FAIL', 'BUILD_FAIL', 'LAUNCH_FAIL',
                        'PASS_ERROR', 'PASS_WARN', 'UNCERTAIN_WARN')
                  for _, s, _, _ in results)
    return 1 if any_bad else 0


if __name__ == '__main__':
    sys.exit(main())
