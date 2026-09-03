# Roadmap (v1.5 - LoongArch64 Bring-Up)

v1.5 adds a minimal LoongArch64 target while preserving the existing RISC-V target. The release has one purpose: boot FrostVista on QEMU LoongArch64 and run user-space programs.

This is a bring-up release, not a LoongArch feature-parity release.

## Phase 1 - Build and Boot <!-- id: phase-1 -->

  - [x] **Architecture selection** <!-- id: architecture-selection -->: add `ARCH=loongarch` without changing the default RISC-V build.
  - [x] **LoongArch toolchain** <!-- id: loongarch-toolchain -->: define the compiler, ABI, and required compiler flags.
  - [x] **Separate build outputs** <!-- id: separate-arch-builds -->: prevent RISC-V and LoongArch objects from sharing build output.
  - [x] **Entry assembly** <!-- id: loongarch-entry -->: set the initial stack, clear BSS, and enter C code.
  - [x] **Linker layout** <!-- id: loongarch-linker -->: define the LoongArch load address, kernel end, and boot stack.
  - [x] **UART output** <!-- id: loongarch-uart -->: initialize QEMU UART and print the kernel banner.
  - [x] **Early trap and timer bring-up** <!-- id: loongarch-early-trap -->: install the kernel trap vector, handle the timer interrupt, and return with `ertn`.

## Phase 2 - Minimal Kernel Runtime <!-- id: phase-2 -->

  - [x] **Basic memory management** <!-- id: loongarch-memory -->: provide the minimal address conversion and page allocation needed by one process. DMW address conversion and the physical memory bounds are defined; the allocator still needs LoongArch validation.
  - [x] **Exception entry** <!-- id: loongarch-exception-entry -->: enter the kernel from a user exception and dispatch the syscall path.
  - [x] **User context setup** <!-- id: loongarch-context-switch -->: construct the initial user context and provide the kernel-to-user transition and user-to-kernel return path.
  - [x] **User address space** <!-- id: loongarch-user-pagetable -->: map one user code region and one user stack.

## Phase 3 - First User Program <!-- id: phase-3 -->

  - [x] **User entry and return** <!-- id: loongarch-user-return -->: enter user mode and return safely using the LoongArch ABI.
  - [x] **One syscall** <!-- id: loongarch-one-syscall -->: implement observable syscalls used by the initial user programs.
  - [x] **User exit** <!-- id: loongarch-user-exit -->: allow user programs to terminate cleanly.
  - [x] **Static user binary** <!-- id: loongarch-static-user -->: build and load statically linked LoongArch64 user programs.

## Validation

  - [x] `make clean && make ARCH=riscv qemu TEST=runner` -> `PASS`
  - [x] `make ARCH=loongarch TEST=fvsh qemu` -> shell starts at `fvsh />`
  - [x] `make ARCH=loongarch TEST=argc qemu` -> `PASS`
  - [x] `python3 ./scripts/run_tests.py --arch loongarch -T 9` -> diskless test set executed
  - [ ] LoongArch diskless test record: `argc`, `wait`, and `while` passed; `brk`, `fork`, `sys_write`, `sys_pipe`, and `lazy_copy` failed; `fault_signal`, `mmap`, `mmap_fork`, and `mmap_lazy` timed out.
  - [x] A clean build of RISC-V and LoongArch does not reuse stale objects.
