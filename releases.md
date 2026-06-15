# 🚀 Roadmap (v1.1 - Virtual Memory Semantics & mmap Milestone)

v1.1 focuses on making FrostVista's user address space model explicit and extensible.
After v1.0 introduced the first interactive shell environment, this milestone moves
the kernel toward real Unix-like virtual memory behavior by adding VMA tracking,
anonymous mmap, munmap, lazy page-fault allocation, and minimal file-backed mappings.

This milestone does not aim to implement full POSIX mmap semantics, shared writable
mappings, copy-on-write fork, mprotect, msync, dynamic linking, or a complete Linux ABI.
The goal is to establish a clean VM foundation that future libc, loader, and process
features can build on safely.

## Phase 1 - Address Space Model
 - [x] **Introduce VMA records**: Track user mappings with fixed-size VMA metadata attached to each process.
 - [ ] **Define the user layout**: Reserve non-overlapping regions for ELF text/data, heap, mmap area, and user stack.
 - [x] **Add VMA helpers**: Provide routines for finding free ranges, detecting overlaps, and inserting mappings.
 - [ ] **Add VMA removal helpers**: Provide routines for removing mappings and releasing VMA slots during `munmap`.
 - [ ] **Keep lifecycle boundaries clear**: Separate process state from address-space state where practical so `fork`, `exec`, and `exit` can reason about VMAs explicitly.

## Phase 2 - Anonymous mmap
 - [x] **Wire the six-argument mmap ABI**: Add the user `mmap()` wrapper, six-argument `ecall` helper, `ARG4`/`ARG5` decoding, and `sys_mmap()` -> `do_mmap()` path.
 - [x] **Implement anonymous private mappings**: Support eager `mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)`.
 - [x] **Choose free virtual ranges**: Allocate page-aligned mmap ranges when the caller passes `addr == NULL`.
 - [x] **Validate unsupported inputs**: Reject invalid lengths, unsupported protections, unsupported flags, non-anonymous file descriptors, and bad offsets.
 - [ ] **Allocate pages lazily**: Extend the user page-fault path so touched anonymous VMA pages are allocated and zero-filled on demand.

## Phase 3 - munmap
 - [ ] **Implement full mapping unmap**: Support unmapping an entire VMA and freeing all mapped physical pages.
 - [ ] **Support edge trimming**: Allow unmapping from the head or tail of an existing VMA when the remaining range is still valid.
 - [ ] **Clear PTEs safely**: Remove page table entries and free backing pages without corrupting unrelated mappings.
 - [ ] **Defer middle splits**: Reject middle-split `munmap` until the VMA code is ready to split one mapping into two.

## Phase 4 - Lifecycle Integration
 - [ ] **Preserve mappings across `fork`**: Copy VMA metadata and eagerly copy already-materialized anonymous pages for the child process.
 - [ ] **Keep lazy ranges lazy**: Preserve untouched VMA ranges across fork without allocating pages until first access.
 - [ ] **Release mappings during `exec`**: Drop all old mmap regions before installing the new executable image.
 - [ ] **Release mappings during `exit`**: Free all mapped pages and VMA metadata when the process exits.

## Phase 5 - File-backed mmap
 - [ ] **Add private read-only file mappings**: Support minimal file-backed `MAP_PRIVATE` mappings for readable files.
 - [ ] **Hold file references**: Keep mapped files alive while VMAs reference them and release those references on unmap/exit/exec.
 - [ ] **Fault in file pages**: Load file data through the page-fault path using page-aligned offsets.
 - [ ] **Reject shared writable mappings**: Defer `MAP_SHARED`, writable file mappings, dirty writeback, and `msync`.

## Phase 6 - Regression Tests
 - [x] **Test basic anonymous mmap**: Verify one-page read/write behavior through the six-argument user interface.
 - [ ] **Expand anonymous mmap tests**: Verify zero-fill, multi-page access, and non-overlap.
 - [ ] **Test lazy faults**: Confirm pages are not allocated until touched and faults inside VMAs are handled.
 - [ ] **Test munmap**: Verify whole unmap, edge trimming, and invalid access after unmap.
 - [ ] **Test lifecycle behavior**: Cover `fork`, `exec`, and `exit` interactions with anonymous mappings.
 - [ ] **Test file-backed reads**: Verify read-only file mappings, page offsets, and EOF/partial-page zero-fill behavior.

## Validation

 - [x] `python3 ./scripts/run_tests.py -t mmap -T 20` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t munmap -T 20` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t mmap_fork -T 20` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t mmap_file -T 20` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t sbrk -T 20 --skip-kernel` -> `PASS`

---

# 🚀 Roadmap (v1.0 - Interactive Shell Milestone)

v1.0 focuses on turning FrostVista from a test-driven kernel into a small interactive Unix-style environment. v0.9 made the local Easy-FS path writable and large-file capable; v1.0 uses that storage foundation together with fork, exec, wait, pipes, and devtmpfs-backed console I/O to build the first FrostVista shell.

This milestone does not aim to implement a full POSIX shell, job control, signals, globbing, quoting, environment variables, or EXT4 write support. The shell should be deliberately small: enough to launch programs, navigate directories, and validate the kernel's process, file descriptor, and pipe paths interactively.

## Phase 1 - Shell Program Skeleton
 - [x] **Add `fvsh` as a user program**: Build a small shell binary with a prompt, line input, command dispatch loop, and clean exit path.
 - [x] **Provide basic line input**: Read newline-terminated commands from stdin and handle empty or whitespace-only lines without disrupting the shell loop.
 - [x] **Keep the shell self-contained**: Reuse `user/ulib.c` syscall wrappers and avoid pulling in a broad libc layer.

## Phase 2 - Built-in Commands
 - [x] **Implement `help` and `exit`**: Provide a discoverable command list and a deterministic way to leave the shell.
 - [x] **Implement `pwd` and `cd`**: Exercise `getcwd` and `chdir` through normal shell commands.
 - [x] **Report failures visibly**: Print command errors to stderr/stdout without panicking the kernel or terminating the shell.

## Phase 3 - External Command Execution
 - [x] **Parse simple argv vectors**: Split command lines into path plus arguments with fixed limits and no quoting.
 - [x] **Run foreground commands**: Use `fork` -> `exec` -> `wait` for external programs and keep the parent shell alive.
 - [x] **Preserve stdio across exec**: Ensure child processes inherit shell stdin, stdout, and stderr correctly.

## Phase 4 - Redirection and Pipes
 - [x] **Support basic redirection**: Implement `cmd > file` and `cmd < file` using `open`, `close`, and `dup3`.
 - [x] **Support one pipeline**: Implement `cmd1 | cmd2` using `pipe2`, two children, descriptor remapping, and parent waits.
 - [x] **Defer complex shell syntax**: Leave append redirection, stderr redirection, multi-stage pipelines, background jobs, and job control for later milestones.

## Phase 5 - Shell Regression Coverage
 - [x] **Add scripted shell tests**: `test_fvsh_script` feeds shell commands from an in-test array instead of manual console input.
 - [x] **Add execution-focused tests**: Verify built-ins, foreground exec, redirection, one pipeline, and redirection-pipe combinations under QEMU.
 - [x] **Preserve existing regressions**: Keep Easy-FS direct/single/double-indirect tests, pipe tests, and EXT4 read-only boot paths passing while shell support lands.

## Validation

 - [x] `python3 ./scripts/run_tests.py -t fvsh_script -T 30` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t easyfs_maxfile -T 20 --skip-kernel` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel` -> `PASS_EXPECTED_LOG`
 - [x] Manual smoke: boot `fvsh`, run `pwd`, an external command, one redirection, one pipe, and `exit`.

---

# 🚀 Roadmap (v0.9 - Easy-FS Completion & Writable VFS Milestone)

v0.9 focuses on making the local Easy-FS backend a reliable writable filesystem. The pipe milestone made process-to-process byte streams real; this milestone makes persistent file data real enough for future shell, redirection, and user workflow work.

This milestone does not aim to make EXT4 writable, add full POSIX mount support, or build an interactive shell. EXT4 remains the read-only contest/root image path. Easy-FS is the writable local backend for regular files, directory updates, and persistence-oriented tests.

v0.9 exceeded its original writable-filesystem scope by adding Easy-FS single-indirect and double-indirect file block mapping. The remaining release work after this milestone is documentation/tooling cleanup, not core v0.9 functionality.

## Phase 1 - VFS Write Contract
 - [x] **Define basic open flag behavior**: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, and invalid `O_TRUNC | O_RDONLY` handling are covered in the VFS path.
 - [x] **Clarify file offset rules**: `read`, `write`, `lseek`, and `dup` offset sharing are verified in `test_easyfs_offset`.
 - [x] **Separate backend capabilities**: Easy-FS exposes writable regular files; EXT4 stays read-only; devtmpfs handles device I/O. Verified in `test_backend`.

## Phase 2 - Easy-FS File Writes
 - [x] **Create regular files**: Support creating a missing regular file through the VFS/open path with `O_CREAT`.
 - [x] **Write file data**: Persist direct-block writes to Easy-FS regular files and preserve correct file size updates.
 - [x] **Support append and truncation**: Implement append-at-end and truncate-to-empty behavior for regular files.
 - [x] **Handle cross-block writes**: Exercise writes that span multiple Easy-FS blocks without corrupting neighboring files.

## Phase 3 - Directory and Path Operations
 - [x] **Allocate directory entries safely**: `dirlink` reuses zeroed dirent slots after unlink; verified in `test_easyfs_dirent`.
 - [x] **Support unlink basics**: Remove regular files and release their inode/data blocks when safe.
 - [x] **Support mkdir basics**: Create directories with correct parent/child path lookup behavior.
 - [x] **Harden path edges**: Empty path, missing parent, non-directory parent, DIRSIZ boundary, and DIRSIZ-1 name acceptance are covered in `test_easyfs_path`.

## Phase 4 - Persistence Regression Tests
 - [x] **Reopen-after-close tests**: Write a file, close it, reopen it, and verify the data and size.
 - [x] **Multi-file allocation tests**: Create and write several files to ensure block allocation does not overlap.
 - [x] **Truncate and append tests**: Verify data after truncation, append, and overwrite sequences.
 - [x] **Unlink tests**: Confirm removed files cannot be reopened and remaining files still read correctly.

## Phase 5 - Userland FS Coverage
 - [x] **Add focused Easy-FS tests**: `test_open` covers open flags, create/write/read, truncate, append, multi-file, cross-block writes, and cross-block append; `test_easyfs_maxfile` covers the direct/single-indirect/double-indirect boundaries; `test_easyfs_indirect` and `test_easyfs_double_indirect` cover explicit indirect-block reads and writes; `test_easyfs_itrunc` covers indirect-block unlink and reuse; `test_easyfs_unlink` covers file deletion and post-unlink allocation; `test_easyfs_mkdir` covers directory creation and subdirectory file I/O; `test_easyfs` covers full integration including nested mkdir and recreate-after-unlink; `test_easyfs_offset` covers lseek, dup, and offset sharing; `test_easyfs_dirent` covers dirent slot reuse; `test_easyfs_path` covers path boundary hardening; `test_backend` covers EXT4 read-only and devtmpfs device capability separation.
 - [x] **Update the Python runner**: Include all Easy-FS and backend tests in the automated list with explicit `ROOTFS` and `FS_LIST` selection.
 - [x] **Preserve existing paths**: `sys_pipe` and `sys_misc` diagnostic allowlists are tightened; EXT4 and devtmpfs regressions are confirmed with the full suite.

## Phase 6 - Easy-FS Indirect Blocks
 - [x] **Adopt the final inode block layout**: Easy-FS uses 10 direct slots, one single-indirect slot, and one double-indirect slot without changing the 64-byte on-disk inode size.
 - [x] **Support large-file mapping**: `bmap()` handles direct, single-indirect, and double-indirect logical block numbers.
 - [x] **Release indirect trees on truncation**: `easyfs_itrunc()` frees direct blocks, indirect data blocks, second-level indirect blocks, and top-level metadata blocks.
 - [x] **Expand the Easy-FS test image**: The Easy-FS formatted space is 4096 blocks, enough to exercise double-indirect allocation paths.

## Validation

 - [x] `python3 ./scripts/run_tests.py -t open -T 20 --skip-kernel` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t easyfs_maxfile -T 20 --skip-kernel` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t easyfs_unlink -T 20 --skip-kernel` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t easyfs_indirect -T 20 --skip-kernel` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t easyfs_double_indirect -T 20 --skip-kernel` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t easyfs_itrunc -T 20 --skip-kernel` -> `PASS`
 - [x] `showfs --check build/disk.img` -> `Healthy` for direct-only Easy-FS images.
 - [ ] `showfs --check build/disk.img` understands indirect-block metadata for images containing `/maxfile`.
 - [x] `showfs --stat 2 build/disk.img` after `easyfs_maxfile` shows `/maxfile` using direct slots plus single/double-indirect metadata slots.

---

# 🚀 Roadmap (v0.8 - Pipe & Unix IPC Milestone)

v0.8 focuses on the first real Unix-style inter-process communication path. The milestone is intentionally centered on anonymous pipes, but the real target is the file descriptor, file object, blocking I/O, and process lifecycle behavior that pipes require.

This milestone does not aim to add a shell pipeline parser, sockets, named FIFOs, `poll`/`select`, EXT4 write support, or broad mount work. It should make simple parent-child pipe communication reliable enough that later shell and IPC work can build on it.

## Phase 1 - File Object Dispatch
 - [x] **Add pipe-backed file objects**: Extend the file type model so file descriptors can refer to either VFS nodes or pipe endpoints.
 - [x] **Make file operations type-aware**: Route read, write, and close through the correct backend without assuming every descriptor owns a VFS inode.
 - [x] **Preserve existing VFS behavior**: Keep regular files and devtmpfs devices working through the same file descriptor paths after pipe support is added.

## Phase 2 - Pipe Buffer and Blocking Semantics
 - [x] **Implement anonymous pipe state**: Add a bounded in-kernel ring buffer with readable and writable endpoint state.
 - [x] **Support blocking reads and writes**: Use the scheduler sleep/wakeup path when readers wait for data or writers wait for space.
 - [x] **Handle EOF and broken-pipe cases**: Return EOF when writers are gone and fail writes when readers are gone.

## Phase 3 - `pipe2` Syscall Integration
 - [x] **Decode `pipe2` arguments**: Add the syscall entry and copy the two allocated file descriptors back to userspace.
 - [x] **Allocate endpoint descriptors safely**: Create two pipe-backed file objects with correct readable/writable permissions.
 - [x] **Harden failure rollback**: Release partially allocated pipe, file table, and fd state on allocation or copyout failure.

## Phase 4 - Process and Descriptor Lifecycle
 - [x] **Verify fork inheritance**: Ensure child processes inherit pipe file descriptors with correct reference counts.
 - [x] **Verify close and dup behavior**: Basic close behavior and dup-based lifetime extension are covered.
 - [x] **Preserve wait/exit behavior**: `wait()` after explicit endpoint close and process-exit fd close are covered.

## Phase 5 - Pipe Regression Tests
 - [x] **Basic pipe transfer**: Test one-process write/read behavior through a pipe.
 - [x] **Fork pipe communication**: Test child-to-parent and parent-to-child communication across `fork`.
 - [x] **Endpoint lifecycle tests**: EOF after closing writers, write failure after closing readers, dup-based lifetime extension, and exit-driven fd close are covered.
 - [x] **Full-buffer wakeup test**: Fill the pipe buffer, wake a writer after reader drain, and verify the final byte.

## Validation

 - [x] `make build_test TEST=sys_pipe`
 - [x] `python3 ./scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel` -> `PASS_EXPECTED_LOG`
 - [x] `python3 ./scripts/run_tests.py --check logs/` keeps `sys_pipe` diagnostics count-limited and expected.

---

# 🚀 Roadmap (v0.7 - Filesystem Architecture & Device Model Milestone)

v0.7 turns the contest-oriented filesystem path from v0.6 into a cleaner multi-filesystem foundation. The milestone focuses on separating generic VFS behavior from filesystem-specific implementation details, introducing a real device filesystem, and retiring the temporary mock `/dev/tty` path left from earlier milestones.

This milestone is primarily architectural. It does not aim to complete full EXT4 write support or broad POSIX mount semantics. Instead, it establishes the boundaries that future filesystem and device work can build on safely.

## Phase 1 - VFS Boundary Cleanup
 - [x] **Clarify generic filesystem responsibilities**: Keep path traversal, file descriptor dispatch, common inode/file abstractions, and mount-point handling in the VFS layer.
 - [x] **Move backend-specific behavior behind filesystem operations**: Ensure Easy-FS, EXT4, and future filesystems provide their behavior through VFS-facing operation tables instead of leaking layout details into generic code.
 - [x] **Remove contest-era shortcuts**: Replace temporary EXT4/Easy-FS branches in generic paths with normal backend dispatch.

## Phase 2 - Filesystem Backend Separation
 - [x] **Make Easy-FS a self-contained backend**: Keep Easy-FS allocation, inode persistence, directory handling, and file data mapping inside the Easy-FS implementation.
 - [x] **Make EXT4 a formal read-only backend**: Preserve the v0.6 contest reader while exposing it through the same VFS-facing model as other filesystems.
 - [x] **Keep shared infrastructure generic**: Restrict common block and inode cache code to filesystem-independent caching, locking, and lifecycle responsibilities.

## Phase 3 - devtmpfs and Device Files
 - [x] **Introduce devtmpfs**: Add an in-memory filesystem for kernel-created device nodes.
 - [x] **Move `/dev/tty` out of the mock VFS tree**: Represent the console as a real device file reachable through normal pathname lookup.
 - [x] **Unify device I/O with file I/O**: Route console read/write through the same file operation path used by regular files.

## Phase 4 - Mount and Boot Integration
 - [x] **Separate root filesystem and device filesystem**: Allow the boot rootfs and `/dev` to come from different filesystem backends.
 - [x] **Preserve existing boot paths**: Keep the Easy-FS fallback, OpenSBI boot path, and EXT4 contest runner working while the architecture is cleaned up.
 - [x] **Prepare for future mount support**: Establish enough internal mount structure to support later user-visible mount and umount work.

## Phase 5 - Validation and Documentation
 - [x] **Regression test core boot flows**: Verify local Easy-FS boot, OpenSBI boot, and EXT4 contest runner behavior after the split.
 - [x] **Regression test device I/O**: Verify stdio and `/dev/tty` behavior through devtmpfs.
 - [x] **Document the new boundaries**: Update roadmap and architecture notes so future filesystem work follows the new VFS/backend split.

---

# 🚀 Roadmap (v0.6 - Contest Bootstrapping Milestone)

v0.6 moves FrostVista from a self-contained Easy-FS demo environment toward the contest evaluator. The milestone is intentionally narrow: boot the submitted `kernel-rv` in the evaluator's RISC-V QEMU environment, read the provided EXT4 test disk, run tests in a controlled sequence, and shut down.

This milestone deliberately defers the full devtmpfs cleanup and broader architecture boundary work. The current mock `/dev/tty` path remains in place until the contest boot and runner path is working.

## Phase 1 - Contest Boot Path
 - [x] **OpenSBI entry**: Support `-bios default -kernel kernel-rv`, where the kernel starts from OpenSBI in S-mode rather than from the local M-mode `-bios none` path.
 - [x] **Local boot compatibility**: Keep `make qemu` usable for fast local runs while making the contest path the release target.
 - [x] **Shutdown validation**: Use SBI SRST under OpenSBI and keep the QEMU `sifive,test` fallback only for local `-bios none` development.
 - [x] **Build artifact contract**: Keep `make all` build-only and make sure it emits `kernel-rv` without launching QEMU.

## Phase 2 - Minimal Read-Only EXT4
 - [x] **EXT4 mount probe**: Detect the evaluator's EXT4 image on virtio disk `x0` without relying on a partition table.
 - [x] **Metadata reader**: Parse the superblock, block group descriptor, inode table location, and root inode.
 - [x] **Root directory scan**: Enumerate root directory entries to discover scripts and executable test files.
 - [x] **Extent-backed reads**: Read regular file contents through the common simple extent cases needed by contest binaries and scripts.

## Phase 3 - ELF Loading From Contest Disk
 - [x] **Reader-based ELF loader**: Split ELF loading from Easy-FS `readi()` so the loader can consume either Easy-FS or EXT4-backed files.
 - [x] **Single binary execution**: Execute one selected ELF directly from EXT4 before adding script discovery.
 - [x] **Preserve Easy-FS fallback**: Keep the current `/init` Easy-FS path for local regression testing while the contest path is developed.

## Phase 4 - Contest Runner
 - [x] **Marker output**: Print the current `basic-musl` group start/end markers exactly in the runner output.
 - [x] **Serial execution**: Run selected tests one at a time through the existing `fork`/`exec`/`wait` lifecycle.
 - [x] **Final shutdown**: Call the shutdown path after the selected runner list finishes.
 - [x] **Embedded init runner**: Generate `build/gen/kernel/init_code.h` from the selected user test and use it as the `/init` image when present.

## Phase 5 - Test-Driven Syscall Fill
 - [x] **Basic syscall expansion**: Add only the syscalls required by failing tests.
 - [x] **Low-risk basic batch**: Pass or exercise the current low-risk runner set: `brk`, `getpid`, `getppid`, `write`, `exit`, `fork`, `wait`, `uname`, `times`, `gettimeofday`, `yield`, and `sleep`.
 - [x] **ABI visibility**: Add Linux RISC-V numbers for the next file, directory, VM, and mount-related tests, with explicit `LOG_ERROR` stubs for calls that are not implemented yet.
 - [x] **File descriptor semantics**: Finish the `close`, `fstat`, `open/openat`, `read`, and `dup3`/`dup2` behavior exposed by the next runner batch.

## Other
 - [x] **Superblock and feature gate**: Read the EXT4 superblock at byte offset 1024, validate magic `0xEF53`, record core layout fields, and reject unsupported incompatible features.
 - [x] **Group descriptor and root inode**: Read group 0's inode table location, load inode #2, and verify that the root directory uses extent-backed storage.
 - [x] **Root directory enumeration**: Parse the root directory's depth-0 extent and print `ext4_dir_entry_2` records from the first directory data block.
 - [x] **Local image target**: Use `make qemu BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner` to boot with an official EXT4 image as virtio `x0`.
 - [x] **EXT4-backed exec path**: Wire EXT4 lookup/read support into the VFS and ELF loading path so `/init` can be resolved from the contest image instead of Easy-FS only.
 - [x] **BusyBox reaches syscall dispatch**: Launch the contest image BusyBox far enough to expose missing syscall coverage as the next blocker.
 - [x] **Restore kernel `tp` on user trap**: Fix the trap entry path where musl uses `tp` as user TLS, while the kernel expects `tp` to hold the hart id for `cpuid()`/`get_cpu()`. On the current single-hart target, `uservec` restores `tp = 0` before entering C trap handling.
 - [x] **Separate DRAM base from kernel load base**: Keep `PHYSTOP_LOW` anchored to QEMU virt RAM at `0x80000000 + 128 MiB`, while allowing the OpenSBI kernel entry/load base to be `0x80200000`.
 - [x] **Fix `kalloc_init` access fault**: Prevent `freerange()` from releasing pages up to `0x88200000`, which incorrectly treated the OpenSBI 2 MiB entry offset as additional RAM and caused `memset()` to store beyond physical memory at `0x88000000`.
 - [x] **Verified OpenSBI boot**: `make qemu BOOT=opensbi LOG=TRACE` reaches `kalloc_init end`, runs the user `argc` test, and enters `sys_shutdown`.
 - [x] **Embedded basic runner smoke**: `test/test_runner.c` now launches selected `/musl/basic/*` binaries from the EXT4 image, prints `basic-musl` markers, and shuts down after the list completes.
 - [x] **Openat ABI correction**: Route syscall 56 through `openat(dirfd, path, flags, mode)` argument decoding instead of the old `open(path, flags)` layout, eliminating the high-address access pattern caused by treating `AT_FDCWD` as a path pointer.
 - [x] **Contest runner path**: Add the minimal boot, filesystem, runner, and shutdown flow needed to scan and execute contest tests.

---

# 🚀 Roadmap (v0.5 - The Cleanup & Consolidation Milestone)

With the file system operational, v0.5 tears down the development scaffolding erected during v0.4 and unifies the codebase under a single, consistent architecture. No new features — this is a pure quality milestone.

The critical architectural debt: `open()` still resolves paths through a mock VFS tree (`vfs_lookup` -> `mock_finddir`) so `/dev/tty` can exist before a real device filesystem is available, while `create()`/`unlink()` use the real Easy-FS (`namei`/`nameiparent`). This split cannot be fully removed until v0.6 introduces devtmpfs.

## v0.6 Preview - devtmpfs
 - [x] **Introduce devtmpfs for device nodes**: Move `/dev/tty` and future device entries into a dedicated device filesystem. This will replace the current hardwired UART handling and allow the remaining mock VFS device scaffolding to be deleted cleanly.

## Phase 1 - VFS Debt Tracking
 - [x] **Defer `open()` path unification until devtmpfs**: Keep the current `/dev/tty` mock path alive until v0.6 can provide device nodes through devtmpfs.
 - [x] **Keep mock VFS infrastructure scoped**: `mock_finddir`, `default_mock_ops`, `create_vfs_inode`, and the `dev_dir`/`tty_file` globals remain temporary compatibility code, not the target VFS design.
 - [x] **Remove `test_vfs()` when devtmpfs lands**: The test only exercises the mock tree, so delete it together with the mock device path.
 - [x] **Drop commented-out O_CREATE block**: The stale draft block in `open()` (`file.c:16-33`) can still be cleaned independently because it is not part of the current `/dev/tty` compatibility path.

## Phase 2 - Magic Number Elimination
 - [x] **FS layout constants**: Define `EASYFS_INODE_BITMAP_BLOCK`, `EASYFS_INODE_BLOCK`, `EASYFS_DATA_START` for block numbers hardcoded as `2`, `3`, `4` in `ialloc()`, `balloc()`, and mount code.
 - [x] **Path buffer**: Replace repeated `128`/`127` with `PATH_MAX` in `vfs.c`, `fs.c`, `sysfile.c`.
 - [x] **Syscall argument offsets**: Replace `argint(0,...)`, `argaddr(1,...)` offset literals with named constants in `syscall.c`.
 - [x] **Printf constants**: Name the `32`, `16`, `60` format buffer sizes in `printf.c`.

## Phase 3 - Code Quality Fixes
 - [x] **Fix typos and spelling errors** across the codebase (struct fields, comments, log messages).
 - [x] **Normalize log levels**: Audit every `LOG_*` call site — errors should use `LOG_ERROR`, warnings `LOG_WARN`, normal operations `LOG_DEBUG` or `LOG_TRACE`. Fix inconsistencies like `sys_write` logging a permissions failure at TRACE while the file-not-found case uses ERROR.
 - [x] **Remove dead declarations** from header files (commented-out structs, unused function prototypes).
 - [x] **Fix inode private_data lifecycle**: Move allocation to `get_inode` and deallocation to `put_inode`, remove from `ilock`/`iunlock`. Eliminate `kalloc` leaks in `exec.c` and `mount.c`.
 - [x] **Move string helpers into core**: Relocate common string/memory routines from `kernel/mm` to `kernel/core`, matching their cross-subsystem role.
 - [x] **Harden `exec()` cleanup**: Ensure `ilock()` error paths release the inode, check `loadseg()` failures, and free the old address space without reporting a successful exec as an error.
 - [x] **Fix Easy-FS inode cleanup hazards**: Correct `itrunc()` so it frees only allocated direct blocks, and avoid copying whole cached inode structures during pathname traversal.
 - [x] **Harden syscall and file I/O edges**: Release `ftable_lock` on `open()` allocation failure, guard syscall logging against invalid numbers, validate file ops before read/write, and normalize negative device I/O returns to `-1`.
 - [x] **Remove VFS declaration warning**: Move `struct vfs_dirent` before `struct vfs_file_ops` so the kernel build no longer emits the forward declaration warning.
 - [x] **Lock documentation**: Document inode reference ownership, inode sleeplock acquire/release contracts, and buffer cache lock ownership for the core Easy-FS and bcache helpers.

---


# 🚀 Roadmap (v0.4 - The File System & I/O Milestone)
With memory and process lifecycles firmly established in v0.3, the v0.4 release bridges the gap to persistent storage and unified Unix I/O. This milestone breaks FrostVistaOS out of the "memory island," introducing a Virtual File System (VFS), block device caching, and standard file descriptors, laying the necessary groundwork for complex applications and future file systems like ext2.

## Phase 1 - Virtual File System (VFS) Abstraction
 - [x] **VFS Interface**: Define generic `inode`, `file`, and `superblock` structures to decouple the core kernel logic from specific file system implementations.
 - [x] **File Descriptor Table**: Implement a per-process FD table within `struct Process` to unify standard I/O, files, and devices under a single integer abstraction.
 - [x] **Core I/O Syscalls**: Implement the foundational Unix I/O interface, including `sys_open`, `sys_read`, `sys_write`, `sys_close`, and `sys_dup`.

## Phase 2 - Block Device & Buffer Cache
 - [x] **VirtIO Disk Driver**: Implement a VirtIO-compliant block device driver for QEMU to handle asynchronous disk read and write requests.
 - [x] **Buffer Cache (Block Cache)**: Develop an LRU-based memory cache for disk blocks to minimize slow I/O operations and manage concurrent block access using spinlocks/sleeplocks.
 - [x] **Interrupt-Driven I/O**: Utilize external interrupts to handle disk responses asynchronously, allowing the CPU to execute other processes instead of spinning.

## Phase 3 - Simple File System (Easy-FS)
 - [x] **On-Disk Layout**: Design a minimal file system backend for the VFS, featuring a superblock, block bitmap, inode array, and data blocks to validate the I/O pipeline.
 - [x] **Directory Operations**: Implement pathname translation, hierarchical directory entry management, and basic file creation/deletion logic.
 - [x] **File Data Management**: Map logical file offsets to physical disk blocks, ensuring safe appending and reading of file content.

## Phase 4 - Inter-Process Communication (IPC)
 - [x] **Standard I/O Redirection**: Link standard input, output, and error (FDs 0, 1, and 2) directly to the UART console driver for interactive user sessions.
 - [ ] **Anonymous Pipes**: (Moved to v0.6) Create a bounded ring-buffer mechanism in memory to allow byte-stream communication between processes.

---


# 🚀 Roadmap (v0.3 - The Userland & Lifecycle Milestone)
With the preemptive scheduler and context isolation achieved in v0.2, the v0.3 release shifts focus to true Unix process semantics, executable loading, and kernel concurrency protection. This transforms FrostVista from a task switcher into a full-fledged application host.

## Phase 1 - Unix Process Lifecycle
 - [x] Process Duplication (sys_fork): Implement the complex logic to deep-copy a parent process's page table, memory layout, and Trapframe into a new child process.
 - [x] Process Termination (sys_exit): Safely tear down a process, free its physical pages, close its resources, and transition it to a ZOMBIE state.
 - [x] Zombie Reaping (sys_wait): Allow parent processes to wait for child termination, fetch exit status, and cleanly scrub the PCB from the process table.
 - [x] Orphan Management: Implement logic to reparent orphaned child processes to the init process when their original parent dies first.
## Phase 2 - Executable Loading (ELF)
 - [x] ELF Format Parser: Write a lightweight parser to validate ELF magic numbers and read Program Headers.
 - [x] The Loader (sys_execve): Destroy the current process's memory space, allocate new pages, and map the executable's .text, .data, and .bss segments into U-mode memory.
 - [x] User Stack Initialization: Dynamically allocate a clean user stack and safely push argc, argv, and the initial stack frame before returning to U-mode.
 - [x] The init Process: Replace the hardcoded test payload with a compiled, standalone initcode loaded directly from memory or a basic RAM disk.
## Phase 3 - Dynamic User Memory
 - [x] Heap Expansion (sys_sbrk): Enable user programs to dynamically request more memory pages at runtime.
 - [x] Memory Accounting: Track the sz (size) of each process to prevent user-space from corrupting memory or growing into kernel space.
 - [x] Lazy Allocation: Modify the page fault trap handler to allocate physical memory only when the user program actually touches the heap, avoiding immediate kalloc overhead.
## Phase 4 - Concurrency & Synchronization
 - [x] Spinlocks (struct spinlock): Implement atomic amoswap-based locks to protect shared kernel data structures (like the memory pool and process array).
 - [x] Interrupt Control: Create reliable push_off() and pop_off() functions to safely disable hardware interrupts when entering critical sections, preventing deadlocks.
 - [x] Sleep & Wakeup Primitives: Implement condition variables to allow processes to sleep while waiting for I/O or child processes, without burning CPU cycles in a spin loop.
---

# 🚀 Roadmap (v0.2 - The Architecture & Process Milestone)

Building upon the solid Self-Hosting Memory Management achieved in v0.1, the v0.2 release will focus on architectural decoupling, introducing multitasking, and bridging the gap between User and Supervisor modes.

## **TODO: Phase 1 - Multi-Arch Refactoring**
- [x] **Hardware Abstraction Layer (HAL)**: Decouple generic kernel logic from hardware-specific instructions.
- [x] **Directory Restructuring**: Split the codebase into generic (`kernel/`, `include/`) and architecture-specific (`arch/riscv/`) directories.
- [x] **Smart Build System**: Upgrade the `Makefile` to dynamically compile sources based on the target architecture (e.g., `make ARCH=riscv`).

## **TODO: Phase 2 - System Call Infrastructure**
- [x] **Syscall Dispatcher**: Implement a robust delegation mechanism to handle `ecall` from U-mode, routing based on the `a7` register.
- [x] **Parameter Passing**: Safely extract arguments passed via user registers (`a0-a5`) into the kernel.
- [x] **First True Syscall (`sys_write`)**: Empower user programs to print messages to the UART terminal through the kernel.

## **TODO: Phase 3 - Process Management (PCB)**
- [x] **Process Control Block (`struct Process`)**: Define the core data structure to manage process state, PID, page tables, and dedicated kernel stacks.
- [x] **Process Allocator (`alloc_process`)**: Automate the creation pipeline (allocating memory, initializing the Trapframe, and setting up the process page table).
- [x] **Context Isolation**: Transition from a hardcoded "Mini User Mode" to dynamically allocated Trapframes for reliable register save/restore operations.

## **TODO: Phase 4 - Preemptive Scheduling**
- [x] **Timer Interrupt Finalization**: Complete the WIP timer interrupt handling to ensure a stable tick rate.
- [x] **Context Switcher (`swtch.S`)**: Write the critical assembly routine to swap CPU callee-saved registers between kernel threads/processes.
- [x] **Round-Robin Scheduler**: Implement the first CPU scheduler to multiplex execution time between multiple concurrent user processes.

## TODO
- [x] Clean up magic, dead code, spelling errors, and notebook-style comments

---

# 🚀 Release: (v0.1 - The Memory Milestone)

The FrostVistaOS kernel has successfully achieved **Self-Hosting Memory Management**, laying down a robust, modern, and secure foundation for future architectural expansion.

## **Completed: Phase 1 - Boot & Hardware Foundations**
- [x] **UART Driver**: Implemented MMIO-based serial output for reliable kernel logging and debugging.
- [x] **Boot Memory Allocator**: Engineered a bump-pointer allocator (`ekalloc`) dedicated to early-stage page table creation and initialization.

## **Completed: Phase 2 - Virtual Memory Architecture**
- [x] **Sv39 Paging**: Fully implemented 3-level page tables strictly compliant with the RISC-V Sv39 hardware standard.
- [x] **Higher-Half Mapping**: Successfully relocated and mapped the entire kernel virtual space to `0xFFFFFFC080000000`.
- [x] **The "Leap of Faith"**: Executed a deterministic and safe execution transition from the physical PC to the high-virtual PC.
- [x] **Mapping Cleanup**: Enforced strict memory isolation by dynamically destroying the early identity mapping scaffolding after boot (`kernel_table[0] = 0;`).
- [x] **Memory Semantics Annotation**: Annotated all core memory functions with explicit address requirements (High/Low VA or PA) to prevent pointer corruption.

## **Completed: Phase 3 - Privilege & Interrupts (The Gateway)**
- [x] **Mini User Mode**: Successfully executed a minimal context switch, dropping privileges from Supervisor (S) mode down to User (U) mode.
- [x] **UART Interrupts Handling**: Enabled and processed asynchronous UART interrupts, paving the way for interactive I/O.
- [x] **Trap & Interrupts Foundation**: Established the architectural baseline for timer and external hardware interrupts handling (WIP towards v0.2).

## TODO

- [x] **Memory Semantics Annotation**: Annotated all core memory functions with strict address requirements (High/Low VA or Physical Address) to prevent pointer corruption.
