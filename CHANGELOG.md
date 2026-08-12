## v1.3 - tmpfs and Writable EXT4 Illusion

FrostVista gains a real in-memory filesystem (`tmpfs`) and layers it as a path-mirrored upper layer inside the EXT4 backend, so the read-only EXT4 image appears writable while the disk itself is never modified. A reboot drops the upper layer and the EXT4 image is unchanged.

### Highlights

- **Standalone tmpfs**: in-memory inode model, directory ops (`lookup`/`create`/`mkdir`/`unlink`), file ops (`read`/`write`/`truncate`), mount at `/tmp`, and `stat` without disk backing.
- **EXT4 writable illusion**: overlay layer (`kernel/fs/ext4fs/mix.c`) with upper-first lookup, mirrored create/mkdir, copy-up on first write, whiteout-based unlink, and merged readdir.
- **Regression coverage**: tmpfs and overlay test suites pass; the EXT4 image stays byte-identical under all write paths.

### Additional

- **slab + kmalloc allocator**: object caches with a general-purpose `kmalloc` (9 size classes); `struct pipe`, `struct context`, and exec argv migrated off page-granular `kalloc`.
- **Copy-on-write fork**: pages shared via `PTE_COW` with per-page refcounts; first write copies, kernel copyout into shared pages handled.
- **Directory listing**: `sys_getdents64` for easyfs, ext4 `readdir` across extents, and a user-side `ls`.
- **Kernel test framework**: `TEST_ASSERT`/`RUN_TEST` macros gated by a `CONFIG_TEST` build flag; runner classified by root filesystem with busybox/lua/libctest groups.
- **syscalls**: `fcntl`/`clock_gettime` (riscv64 ABI number 113) and `readv`/`writev` iovec scatter-gather.
- **Hardening**: inode cache keyed on `(dev, ino)`, VMA coverage in copyin/copyout, exec stack and auxv fixes, spurious external interrupt root-caused and fixed, virtio feature negotiation aligned with xv6.

### Validation

- `python3 ./scripts/run_tests.py -t tmpfs --rootfs ext4 --fs-list "ext4 tmpfs devtmpfs" -T 20` -> `PASS`
- `python3 ./scripts/run_tests.py -t overlay --rootfs ext4 --fs-list "ext4 tmpfs devtmpfs" -T 20` -> `PASS`
- Full ext4 suite: 16 PASS + 6 PASS_EXPECTED_LOG, including `backend` re-enabled with overlay semantics.
