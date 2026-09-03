## v1.5.0 - LoongArch64 Bring-Up

FrostVista can now boot directly on QEMU LoongArch64 and run embedded LoongArch64 user programs. This is a bring-up release and does not provide LoongArch feature parity with RISC-V or block-device filesystem support.

### Highlights

- **Direct boot**: LoongArch startup initializes high-half mappings, UART, timers, traps, TLB refill, process context switching, and user return.
- **First user programs**: embedded static LoongArch64 images can execute through the syscall path, including the `fvsh` shell and `argc` test.
- **Diskless runtime**: LoongArch builds use `tmpfs` and `devtmpfs` without a block device or EasyFS/EXT4 image.
- **Build and debug support**: LoongArch QEMU run/debug targets and a configurable `GDB_PORT` are available alongside the RISC-V targets.
- **Architecture-aware tests**: `run_tests.py` separates the RISC-V and LoongArch automated test sets and cleans stale QEMU instances per target.

### Additional

- **Test runner updates**: `run_tests.py` now accepts `--arch loongarch`, uses `ALL_RISCV_TEST` and `ALL_LOONGARCH_TEST` for architecture-specific selection, defaults LoongArch to `bare` boot with `tmpfs devtmpfs`, and cleans stale LoongArch QEMU processes.

### Validation

- `make ARCH=loongarch TEST=fvsh qemu` -> shell starts at `fvsh />`
- `make ARCH=loongarch TEST=argc qemu` -> `PASS`
- `make -B ARCH=loongarch ROOTFS=tmpfs FS_LIST="tmpfs devtmpfs" check-direct-boot` -> `PASS`
- `python3 ./scripts/run_tests.py --arch loongarch -T 9` -> 3 `PASS`, 5 `FAIL`, and 4 `TIMEOUT`
