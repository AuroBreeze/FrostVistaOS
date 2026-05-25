# FrostVista OS / 霜见内核

```text
[INFO]     ______                __ _    ___      __       
[INFO]    / ____/________  _____/ /| |  / (_)____/ /_____ _
[INFO]   / /_  / ___/ __ \/ ___/ __/ | / / / ___/ __/ __ `/
[INFO]  / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / 
[INFO] /_/   /_/   \____/____/\__/ |___/_/____/\__/\__,_/
[INFO] FrostVistaOS booting...
[INFO] Enable time interrupts...
[INFO] Timer init done
[INFO] Paging enable successfully
[INFO] kalloc_init start
[INFO] Total Memory Pages: 32621
[INFO] kalloc_init end
[INFO] clear low memory mappings
[INFO] clear low memory mappings done
[INFO] Hello FrostVista OS!
```

FrostVista is a lightweight, educational operating system kernel targeting **RISC-V 64 (Sv39)**.

---

# Roadmap (v0.6 - Contest Bootstrapping Milestone)

v0.6 is focused on entering the contest evaluation loop as quickly as possible. The goal is not to complete a broad architecture rewrite; it is to make `kernel-rv` boot under the contest QEMU command, discover the provided EXT4 test disk, execute tests serially, print the required markers, and shut down cleanly.

The current `/dev/tty` path still depends on the temporary mock VFS tree. That cleanup remains important, but it is not the first blocker for contest readiness. v0.6 keeps the mock console path stable while adding only the filesystem and boot pieces needed to run the tests.

## Phase 1 - Contest Boot Path
 - [x] **Boot under OpenSBI**: Support the contest `-bios default -kernel kernel-rv` path, where OpenSBI enters the kernel in S-mode.
 - [x] **Preserve local QEMU workflow**: Keep the current `-bios none` development path usable for fast local testing.
 - [x] **Validate shutdown under OpenSBI**: Use SBI SRST as the primary shutdown path, with the QEMU `sifive,test` fallback only for local `-bios none` runs.
 - [x] **Keep `make all` build-only**: Ensure `make all` produces `kernel-rv` without launching QEMU.

## Phase 2 - Minimal Read-Only EXT4
 - [x] **Mount the contest disk**: Detect the EXT4 filesystem on virtio disk `x0`, which the evaluator provides without a partition table.
 - [x] **Read EXT4 metadata**: Parse the superblock, block group descriptor, inode table location, and root inode.
 - [x] **Enumerate root directory entries**: List root-level files so the kernel can discover test scripts and binaries.
 - [x] **Read regular files through extents**: Implement enough extent traversal to load root-level ELF files and script contents.

## Phase 3 - ELF Loading From Contest Files
 - [x] **Abstract ELF input reads**: Decouple the ELF loader from Easy-FS `readi()` by loading through a small file-reader interface.
 - [x] **Load one EXT4 ELF**: Prove the kernel can execute a single user ELF read from the contest disk.
 - [ ] **Keep Easy-FS init available**: Preserve the current Easy-FS `/init` path as a local development fallback while EXT4 support matures.

## Phase 4 - Contest Runner
 - [ ] **Scan for test scripts**: Find root-level `*_testcode.sh` entries on the EXT4 disk.
 - [ ] **Print required markers**: Emit the contest group start/end lines exactly as the evaluator expects.
 - [ ] **Run tests serially**: Execute one test at a time through `fork`/`exec`/`wait` or a minimal equivalent path.
 - [ ] **Shutdown after completion**: Call the shutdown path after all selected tests finish so QEMU exits promptly.

## Phase 5 - Test-Driven Syscall Fill
 - [ ] **Add syscalls only when tests require them**: Prioritize failures observed from actual contest tests over speculative POSIX coverage.
 - [ ] **Likely early syscalls**: `getpid`, `yield`, `sleep`, `waitpid`, `dup2`, `pipe`, `fstat`, `getdents`, `chdir`, `getcwd`, `mkdir`, and `unlink`.
 - [ ] **Keep compatibility narrow**: Implement the behavior needed for the tests first, then harden semantics once the basic groups run.

## Deferred Architecture Work
 - [ ] **devtmpfs and mount model**: Move `/dev/tty` out of the mock VFS after the contest boot path is stable.
 - [ ] **Architecture boundary cleanup**: Add small arch hooks for syscall ABI access, address-space switching, and VM permissions after the contest loop works.
 - [ ] **Filesystem backend split**: Separate VFS-facing operations from Easy-FS block mapping before growing full EXT4 support beyond the minimal reader.

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

**To build and launch QEMU:**

```bash
make run
```

You should see the kernel enabling paging and jumping to the higher half address space in the serial console.

For the OpenSBI path used by the contest-style boot flow:

```bash
make run-sbi
```

To probe a local contest EXT4 image as virtio disk `x0`:

```bash
make run-sbi-ext4 EXT4_IMG=sdcard-rv.img
```

## Philosophy

* **Clarity over Cleverness**: Code is written to be understood.
* **Architecture First**: Implementing proper OS concepts (Virtual Memory, Traps) rather than hacking features.
* **From Scratch**: Minimizing external dependencies to understand the hardware.

---

## Acknowledgments

In its early development stages, FrostVista OS drew significant inspiration from the **xv6** operating system developed by MIT.  
We thank the xv6 authors for their clear, educational implementation of Unix‑like kernel concepts, which laid the foundation for our understanding of file systems, process management, and device drivers.  
The xv6 source code and accompanying textbook (https://pdos.csail.mit.edu/6.828/2023/xv6.html) served as a primary reference throughout the initial design and implementation of FrostVista.
