# Roadmap (v1.6 - LoongArch64 Runtime Parity)

v1.6 focuses on turning the LoongArch64 bring-up into a reliable user-space runtime.

The release should close the current process, virtual-memory, and filesystem gaps while preserving the existing RISC-V test baseline.

## Phase 1 - LoongArch64 Runtime <!-- id: runtime-phase-1 -->

  - [x] **Process lifecycle** <!-- id: runtime-process-lifecycle -->: make `fork`, `exec`, and `wait` reliable on LoongArch64. Normal fork/exec/wait and exit-status tests pass; wait-option validation and stopped-child reporting remain incomplete.
  - [x] **Memory growth** <!-- id: runtime-memory-growth -->: implement and validate `brk` and the required address-space growth paths.
  - [x] **Copy-on-access memory** <!-- id: runtime-lazy-copy -->: fix lazy page copying, page faults, and fork-related virtual-memory paths.
  - [x] **Memory mapping** <!-- id: runtime-mmap -->: bring `mmap`, lazy mapping, and mmap-after-fork to the LoongArch64 test baseline.
  - [x] **Fault and signal handling** <!-- id: runtime-fault-signal -->: report user faults correctly and terminate or signal processes without hangs. Architecture-specific `rt_sigreturn` trampolines are linked into tests and user applications; signal round-trip tests pass on both architectures.
  - [x] **Pipe and output paths** <!-- id: runtime-pipe-output -->: validate `sys_write` and pipe blocking, wakeup, close, and EOF behavior.
  - [x] **UART interrupts** <!-- id: runtime-uart-interrupts -->: route UART RX/TX through PCH-PIC and EIOINTC, buffer console input/output, and deliver SIGINT for Ctrl+C. Keep polling output available during early boot and with interrupts disabled.

## Phase 2 - LoongArch64 Storage <!-- id: storage-phase-2 -->

  - [x] **VirtIO block device** <!-- id: storage-virtio-block -->: enable modern VirtIO PCI block I/O with PCI INTx routing, completion interrupts, and sleep/wakeup on the LoongArch64 QEMU target. Validate disk readback with `pci_irq`.
  - [x] **EasyFS root filesystem** <!-- id: storage-easyfs -->: mount an architecture-specific EasyFS root image, install user applications, and validate disk-backed application execution. LoongArch64 defaults to EasyFS; `/init` still prefers the embedded test image when available.
  - [x] **Shell image** <!-- id: storage-shell-image -->: start `fvsh` and its standard user programs from the LoongArch64 root filesystem. `/fvsh` and the standard applications are packaged; shell interaction and disk-backed `/echo` work. Direct filesystem-based shell startup remains to be validated.

## Phase 3 - Regression and Release Quality <!-- id: quality-phase-3 -->

  - [x] **Architecture test matrix** <!-- id: quality-test-matrix -->: run the RISC-V EasyFS/EXT4 and LoongArch64 tmpfs/EasyFS configurations in CI. The runner now shares the same test catalog across architectures; the complete CI matrix remains pending.
  - [x] **Failure diagnostics** <!-- id: quality-diagnostics -->: save logs by architecture, test, and boot mode; return failure for timeouts, unexpected kernel diagnostics, and uncertain results.
  - [x] **Portable syscall tests** <!-- id: quality-portable-syscalls -->: use architecture syscall helpers in `getuid`, `set_tid_address`, and `fstat` instead of hardcoded RISC-V assembly.
