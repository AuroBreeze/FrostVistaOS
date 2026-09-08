# Roadmap (v1.6 - LoongArch64 Runtime Parity)

v1.6 focuses on turning the LoongArch64 bring-up into a reliable user-space runtime.

The release should close the current process, virtual-memory, and filesystem gaps while preserving the existing RISC-V test baseline.

## Phase 1 - LoongArch64 Runtime <!-- id: runtime-phase-1 -->

  - [ ] **Process lifecycle** <!-- id: runtime-process-lifecycle -->: make `fork`, `exec`, and `wait` reliable on LoongArch64.
  - [ ] **Memory growth** <!-- id: runtime-memory-growth -->: implement and validate `brk` and the required address-space growth paths.
  - [ ] **Copy-on-access memory** <!-- id: runtime-lazy-copy -->: fix lazy page copying, page faults, and fork-related virtual-memory paths.
  - [ ] **Memory mapping** <!-- id: runtime-mmap -->: bring `mmap`, lazy mapping, and mmap-after-fork to the LoongArch64 test baseline.
  - [ ] **Fault and signal handling** <!-- id: runtime-fault-signal -->: report user faults correctly and terminate or signal processes without hangs.
  - [ ] **Pipe and output paths** <!-- id: runtime-pipe-output -->: validate `sys_write` and pipe blocking, wakeup, close, and EOF behavior.

## Phase 2 - LoongArch64 Storage <!-- id: storage-phase-2 -->

  - [ ] **VirtIO block device** <!-- id: storage-virtio-block -->: enable block I/O on the LoongArch64 QEMU target.
  - [ ] **EasyFS root filesystem** <!-- id: storage-easyfs -->: boot LoongArch64 from an EasyFS image instead of an embedded or diskless init program.
  - [ ] **Shell image** <!-- id: storage-shell-image -->: start `fvsh` and its standard user programs from the LoongArch64 root filesystem.

## Phase 3 - Regression and Release Quality <!-- id: quality-phase-3 -->

  - [ ] **Architecture test matrix** <!-- id: quality-test-matrix -->: run the RISC-V EasyFS/EXT4 and LoongArch64 tmpfs/EasyFS configurations in CI.
  - [ ] **Failure diagnostics** <!-- id: quality-diagnostics -->: make QEMU timeouts and kernel faults identify the failing test and path.
  - [ ] **Rust boundary validation** <!-- id: quality-rust-boundary -->: keep the Rust console and allocator integration build- and panic-safe without making Rust feature expansion a release blocker.

## v1.6 Acceptance Criteria

  - [ ] The LoongArch64 diskless test set has no failures or timeouts.
  - [ ] LoongArch64 boots `fvsh` from an EasyFS image on QEMU.
  - [ ] RISC-V regression tests remain green for EasyFS and EXT4.
  - [ ] Clean builds do not reuse objects across architectures.
