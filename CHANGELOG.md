## v1.6.0 - LoongArch64 Runtime and PCI Storage

This release improves process execution, virtual memory, and signal handling on LoongArch64, and adds VirtIO PCI block devices and EasyFS support so that more user programs can run on QEMU.

### Highlights

- **User-space runtime**: improve process execution, memory growth, lazy allocation, copy-on-write, and memory mapping; support signal handling and return; validate pipe I/O, blocking, and wakeups.
- **VirtIO PCI block devices**: implement the modern VirtIO PCI transport backend, route PCI INTx through PCH-PIC and EIOINTC, and support interrupt-driven block I/O.
- **EasyFS and user applications**: default LoongArch64 to EasyFS, generate filesystem images separately for each architecture, and install `fvsh` and common user programs for disk-backed application execution.

### Additional

- **VMA fault fixes**: correct address classification for mappings below the ELF heap and fault handling when system calls first access lazily allocated mappings.
- **UART and Ctrl+C**: align LoongArch64 UART RX/TX buffering and interrupt handling with RISC-V, and deliver SIGINT to terminal foreground processes when Ctrl+C is received.
- **Scheduling adjustments**: remove unconditional yields after system calls on RISC-V and LoongArch64 while retaining timer-driven preemption and explicit yielding.
- **Signal return code**: split `__restore.S` by architecture, add the LoongArch64 `rt_sigreturn` trampoline, and include it in test and user application builds.
- **Automated test synchronization**: share the test catalog across architectures and unify filesystem selection, log naming, and failure classification; return failure for timeouts and uncertain results.
- **Portable syscall tests**: replace hardcoded RISC-V assembly in `getuid`, `set_tid_address`, and `fstat` tests with architecture syscall helpers.
- **Build and image configuration**: select the default VirtIO transport by architecture and separate EasyFS output directories and EXT4 image paths.
- **Wait compatibility**: add the `WUNTRACED` definition and combined-option tests. Option validation is currently bypassed, and stopped-child reporting remains unimplemented.

### Validation

- `getuid`, `set_tid_address`, `fstat`, `signal`, `wait`, `mmap`, and `fvsh_script` pass on both architectures; RISC-V `mmap` includes expected diagnostic output.
- User application builds and `user_apps` pass on both architectures, validating ELF architecture and disk-backed application execution.
- LoongArch64 `pci_irq`, Ctrl+C handling, and interactive UART input checks pass.
- The complete test matrix, direct disk-based `/fvsh` startup, cross-architecture clean builds, and Rust boundary validation remain pending; see `releases.md`.
