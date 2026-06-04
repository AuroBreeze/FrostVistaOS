# FrostVista OS / 霜见内核

```text
[   0.094] [ INFO] Paging enable successfully
------------------------------------------------------------
    ______                __ _    ___      __       
   / ____/________  _____/ /| |  / (_)____/ /_____ _
  / /_  / ___/ __ \/ ___/ __/ | / / / ___/ __/ __ `/
 / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / 
/_/   /_/   \____/____/\__/ |___/_/____/\__/\__,_/

RISC-V 64  |  Sv39  |  v0.7
------------------------------------------------------------
[   0.101] [ INFO] Enable time interrupts...
[   0.102] [ INFO] Timer init done
------------------------------------------------------------
  ◆ Platform Init
[   0.104] [ INFO] kalloc_init start
[   0.672] [ INFO] Total Memory Pages: 32039
[   0.673] [ INFO] kalloc_init end
[   0.673] [ INFO] clear low memory mappings
[   0.674] [ INFO] clear low memory mappings done
[   0.675] [ INFO] Hello FrostVista OS!
------------------------------------------------------------
  ◆ Process Subsystem
------------------------------------------------------------
  ◆ Filesystem & Devices
[   0.679] [ INFO] virtio-blk initialized, mmio version 2
------------------------------------------------------------
```


FrostVista is a compact **RISC-V 64 (Sv39)** kernel shaped by a simple idea:
keep the system small, but let every boundary be real.

It is built for learning, experimentation, and small embedded-style
environments. The kernel favors clear structure, direct code, and working
system paths over broad compatibility or unnecessary abstraction.

---

## Current Milestone (v1.0 - Interactive Shell Milestone)

v1.0 focuses on turning FrostVista from a test-driven kernel into a small interactive Unix-style environment. v0.9 made the local Easy-FS path writable and large-file capable; v1.0 uses that storage foundation together with fork, exec, wait, pipes, and devtmpfs-backed console I/O to build the first FrostVista shell.

This milestone does not aim to implement a full POSIX shell, job control, signals, globbing, quoting, environment variables, or EXT4 write support. The goal is a compact shell that can read commands from `/dev/tty`, run user programs, navigate the filesystem, and exercise simple redirection and pipe workflows.

### Phase 1 - Shell Program Skeleton
 - [ ] **Add `fvsh` as a user program**: Build a small shell binary with a prompt, line input, command dispatch loop, and clean exit path.
 - [ ] **Provide basic line editing behavior**: Accept newline-terminated commands from stdin and handle empty lines without disrupting the shell loop.
 - [ ] **Keep shell code self-contained**: Reuse `test/ulib.c` syscall wrappers without adding broad libc dependencies.

### Phase 2 - Built-in Commands
 - [ ] **Implement `help` and `exit`**: Provide a discoverable command list and a deterministic way to leave the shell.
 - [ ] **Implement `pwd` and `cd`**: Exercise `getcwd` and `chdir` through normal shell commands.
 - [ ] **Report failures visibly**: Print clear command errors without panicking the kernel or terminating the shell.

### Phase 3 - External Command Execution
 - [ ] **Parse simple argv vectors**: Split command lines into path plus arguments with fixed limits and no quoting.
 - [ ] **Run foreground commands**: Use `fork` -> `exec` -> `wait` for external programs and keep the parent shell alive.
 - [ ] **Preserve stdio across exec**: Ensure child processes inherit shell stdin, stdout, and stderr correctly.

### Phase 4 - Redirection and Pipes
 - [ ] **Support basic redirection**: Implement `cmd > file` and `cmd < file` using `open`, `close`, and `dup3`.
 - [ ] **Support one pipeline**: Implement `cmd1 | cmd2` using `pipe2`, two children, descriptor remapping, and parent waits.
 - [ ] **Defer complex shell syntax**: Keep append redirection, stderr redirection, multi-stage pipelines, and background jobs out of v1.0.

### Phase 5 - Shell Regression Coverage
 - [ ] **Add shell parser tests**: Cover empty input, built-ins, argument splitting, redirection syntax, and one-pipe syntax.
 - [ ] **Add shell execution tests**: Verify built-ins, foreground exec, redirection, and one pipeline under QEMU.
 - [ ] **Keep v0.9 storage regressions passing**: Preserve the Easy-FS direct/single/double-indirect tests while shell support lands.

---

## Memory Layout

FrostVista utilizes the Sv39 virtual addressing scheme:

```text
0xFFFFFFC080000000  ->  Kernel Base (Virtual)
        |                   maps to
0x0000000080000000  ->  Physical RAM Start
```

Under OpenSBI, the kernel is loaded at `0x80200000`, but QEMU virt RAM
still starts at `0x80000000` and spans 128 MiB. The kernel therefore keeps
the physical memory limit based on the DRAM base:

```text
DRAM_BASE_LOW   = 0x80000000
KERNEL_BASE_LOW = 0x80200000  // OpenSBI boot
PHYSTOP_LOW     = DRAM_BASE_LOW + 128 MiB = 0x88000000
```

This avoids treating the OpenSBI kernel entry offset as extra RAM.

## Build & Run

**Requirements:**

* `riscv64-elf-gcc` (or similar cross-compiler)
* `qemu-system-riscv64`
* `make`

**To build and launch QEMU with the default local configuration:**

```bash
make qemu
```

You should see the kernel enabling paging and jumping to the higher half address space in the serial console. The `qemu` target respects explicit build parameters, so use it as the normal hand-written run entry point.

Useful parameters:

```text
BOOT=bare|opensbi
ROOTFS=easyfs|ext4
FS_LIST="easyfs devtmpfs"|"ext4 devtmpfs"
TEST=<test name under test/test_*.c, without the test_ prefix>
BUILD=release|debug
```

For the OpenSBI EXT4 runner path:

```bash
make qemu BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner BUILD=debug
```

For a paused GDB session on the same path:

```bash
make debug BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner
make gdb
```

## Automated Tests

The Python runner builds one user test at a time, launches QEMU, records logs under `logs/`, and classifies kernel diagnostics. Expected diagnostics from negative syscall tests are reported as `PASS_EXPECTED_LOG`; unexpected `[WARN]` or `[ERROR]` lines are surfaced separately. `sys_pipe` also count-limits its expected diagnostics so extra matching warnings are not silently accepted.

```bash
python3 ./scripts/run_tests.py --list
python3 ./scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t easyfs -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t backend -T 20 --skip-kernel --rootfs ext4 --fs-list "ext4 devtmpfs"
python3 ./scripts/run_tests.py --check logs/
```

The Easy-FS writable-path tests (`open`, `easyfs_*`) automatically select `ROOTFS=easyfs` and `FS_LIST="easyfs devtmpfs"`. The `backend` test runs on `ROOTFS=ext4` with devtmpfs to confirm capability separation.

Current focused regression tests include:

```text
sbrk fork sys_write sys_misc sys_pipe open easyfs_maxfile easyfs_unlink easyfs_mkdir easyfs easyfs_offset easyfs_dirent easyfs_path backend io vfs lazy_copy runner
```

## Philosophy

* **Elegant Simplicity**: Small code, clear shape, real behavior.
* **Real Boundaries**: Keep the kernel compact while preserving true OS structure.
* **Working System First**: Make paths boot, run, read, write, and fail visibly.
* **Purposeful Abstraction**: Abstract only when it makes the system simpler to grow.
* **Classic Roots, Own Path**: Learn from xv6, but let FrostVista become its own kernel.

---

## Acknowledgments

In its early development stages, FrostVista OS drew significant inspiration from the **xv6** operating system developed by MIT.  
We thank the xv6 authors for their clear, educational implementation of Unix‑like kernel concepts, which laid the foundation for our understanding of file systems, process management, and device drivers.  
The xv6 source code and accompanying textbook (https://pdos.csail.mit.edu/6.828/2023/xv6.html) served as a primary reference throughout the initial design and implementation of FrostVista.
