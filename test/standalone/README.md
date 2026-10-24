# Standalone Tests

This directory contains user-space regression tests, interactive demos, and
external-suite drivers. Each selected program is built separately and loaded as
the guest's `/init`. Programs implement `int main(int argc, char **argv)`.
The shared CRT supplies `_start()`, calls `main()`, and passes its return value
to `exit()`. Most standalone regressions shut down the guest after reporting
their results instead of returning from `main()`.

The startup implementation lives in [`user/rime/crt.c`](../../user/rime/crt.c)
and is linked into both tests and applications through `mk/rime.mk`. Keep
`shutdown()` at the end of independently booted regressions: returning from
`main()` ends only that process, not the QEMU run.

These programs exercise kernel behavior through the user-space runtime. Shared
declarations and test macros live in
[`include/user/user.h`](../../include/user/user.h) and
[`include/user/libtest.h`](../../include/user/libtest.h).

## Build and Run

Run commands from the repository root. The host needs Python 3, GNU Make, the
selected architecture's cross compiler, and QEMU. The test runner uses a Unix-like
host environment; on Windows, run it through WSL.

List the tests supported by each architecture:

```sh
python3 scripts/run_tests.py -a riscv --list
python3 scripts/run_tests.py -a loongarch --list
```

Run an individual regression with a 30-second timeout:

```sh
python3 scripts/run_tests.py -a riscv -t time --rootfs easyfs -T 30
python3 scripts/run_tests.py -a loongarch -t time -T 30
python3 scripts/run_tests.py -a riscv -t fvsh_script --rootfs easyfs -T 30
```

The runner defaults to OpenSBI boot on RISC-V and bare boot with tmpfs on
LoongArch. RISC-V common tests default to EXT4 unless `--rootfs` is supplied;
using `--rootfs easyfs` selects the locally generated EasyFS image instead.
LoongArch currently supports the diskless subset listed by `--list`.

Use `-v` to stream guest output, or inspect saved logs:

```sh
python3 scripts/run_tests.py -a riscv -t time --rootfs easyfs -T 30 -v
python3 scripts/run_tests.py --check logs/ --rootfs easyfs
```

Logs are named `<arch>_<test>_<boot>.log` under `logs/` by default. Use `-o DIR`
to select another log directory. When checking existing logs, select the rootfs
used for that run, particularly when interpreting filesystem-specific failures.

For an interactive shell, use the Make entry point:

```sh
make qemu ARCH=riscv ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=fvsh
```

`TEST=<name>` selects `test/standalone/test_<name>.c`. The build produces
`build/<arch>/test/init_bin` and embeds it through
`build/<arch>/gen/kernel/init_code.h`. Moving a source into this directory does
not change its `TEST` name.

## Test Areas

| Area | Examples |
| --- | --- |
| Process lifecycle and arguments | `argc`, `fork`, `wait`, `execv` |
| Memory and mappings | `brk`, `lazy_copy`, `mmap`, `mmap_fork`, `mmap_lazy` |
| Mapping lifecycle and file backing | `mmap_execve`, `mmap_exit`, `mmap_file` |
| Syscalls, signals, and time | `sys_misc`, `sys_write`, `sys_pipe`, `signal`, `fault_signal`, `time` |
| Filesystem operations | `open`, `fstat`, `vfs`, `easyfs_*`, `tmpfs`, `overlay` |
| Shell regression | `fvsh_script` |
| Interactive demos | `echo`, `fvsh`, `init` |
| External-suite driver | `runner` |

The authoritative execution groups and architecture allowlists are maintained in
[`scripts/run_tests.py`](../../scripts/run_tests.py). A source file's presence
does not imply that it is registered, supported on both architectures, or ready
for unattended execution. For example, `readloop` is currently unclassified and
is not automatically included in batch regressions.
The `runner` driver also contains suite calls that are currently disabled.

## Results and Diagnostics

Automated tests use the macros from `user/libtest.h` to emit markers:

```text
=== TEST time ===
=== PASS: time ===
```

Failed assertions print a diagnostic and an `=== FAIL: <name> ===` marker before
shutting down. Negative tests may deliberately trigger kernel warnings. Register
only the specific expected diagnostics in the host runner; unexpected warnings,
errors, and panics require investigation even when a PASS marker is present.
`PASS_EXPECTED_LOG` means the run passed with recognized expected diagnostics.

## Add a Standalone Regression

1. Create `test_<name>.c` in this directory and include `user/user.h` and
   `user/libtest.h`.
2. Provide `int main(int argc, char **argv)`, emit a top-level `TEST_START`, run
   focused checks, emit `TEST_PASS`, and call `shutdown()` when finished. A file
   can contain several related checks, but it remains one independently booted
   program.
3. Register the name in `COMMON_TESTS`, `EASYFS_TESTS`, or `EXT4_TESTS` in the host
   runner according to its dependencies. Add it to `ALL_LOONGARCH_TEST` only
   after validating that architecture. Interactive demos belong in
   `MANUAL_TESTS`.
4. For error paths, verify the return value and any expected kernel diagnostic.
   Release files, mappings, descriptors, and child processes between checks.
5. Run the test on its supported configurations and inspect the saved log.

Unregistered source files appear under `unclassified` in `--list`; they are not
automatically included in batch regressions. These standalone programs can remain
available while a future shared test framework is introduced alongside them.
