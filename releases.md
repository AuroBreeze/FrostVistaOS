# Roadmap (v1.5 - LoongArch64 Bring-Up)

v1.5 adds a minimal LoongArch64 target while preserving the existing RISC-V target. The release has one purpose: boot FrostVista on QEMU LoongArch64 and run one user-space program.

This is a bring-up release, not a LoongArch feature-parity release.

## Phase 1 - Build and Boot <!-- id: phase-1 -->

  - [ ] **Architecture selection** <!-- id: architecture-selection -->: add `ARCH=loongarch64` without changing the default RISC-V build.
  - [ ] **LoongArch toolchain** <!-- id: loongarch-toolchain -->: define the compiler, ABI, and required compiler flags.
  - [ ] **Separate build outputs** <!-- id: separate-arch-builds -->: prevent RISC-V and LoongArch objects from sharing build output.
  - [ ] **Entry assembly** <!-- id: loongarch-entry -->: set the initial stack, clear BSS, and enter C code.
  - [ ] **Linker layout** <!-- id: loongarch-linker -->: define the LoongArch load address, kernel end, and boot stack.
  - [ ] **UART output** <!-- id: loongarch-uart -->: initialize QEMU UART and print the kernel banner.

## Phase 2 - Minimal Kernel Runtime <!-- id: phase-2 -->

  - [ ] **Basic memory management** <!-- id: loongarch-memory -->: provide the minimal address conversion and page allocation needed by one process.
  - [ ] **Exception entry** <!-- id: loongarch-exception-entry -->: enter the kernel from a user exception and dispatch the syscall path.
  - [ ] **User context setup** <!-- id: loongarch-context-switch -->: construct the initial user context and provide the kernel-to-user transition and user-to-kernel return path.
  - [ ] **User address space** <!-- id: loongarch-user-pagetable -->: map one user code region and one user stack.

## Phase 3 - First User Program <!-- id: phase-3 -->

  - [ ] **User entry and return** <!-- id: loongarch-user-return -->: enter user mode and return safely using the LoongArch ABI.
  - [ ] **One syscall** <!-- id: loongarch-one-syscall -->: implement one observable syscall, preferably console `write`.
  - [ ] **User exit** <!-- id: loongarch-user-exit -->: allow the first user program to terminate cleanly.
  - [ ] **Static user binary** <!-- id: loongarch-static-user -->: build and load one statically linked LoongArch64 user program.

## Validation

  - [ ] `make clean && make ARCH=riscv qemu TEST=runner` -> `PASS`
  - [ ] `make clean && make ARCH=loongarch64 qemu TEST=<minimal-user-test>` -> kernel boots and the user program exits successfully
  - [ ] `python3 ./scripts/run_tests.py --arch loongarch64 -t <minimal-user-test> -T 20` -> `PASS`
  - [ ] A clean build of RISC-V and LoongArch64 does not reuse stale objects.
