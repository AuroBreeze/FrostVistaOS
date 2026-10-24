# User-Space Test Framework Entry Point

`test.c` is the entry point for the new test framework. It currently executes
one million `getpid()` calls, reports the total elapsed time and average time
per call, and then shuts down the guest.

Run the following command from the repository root in an environment with the
cross toolchain and QEMU installed:

```bash
make qemu ARCH=riscv ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=framework BUILD=release
```

`TEST=framework` selects `test/framework/test.c`; other `TEST=<name>` values
still select `test/standalone/test_<name>.c`. Both the RISC-V and LoongArch build
rules support this entry point. Which test cases can actually run depends on
the kernel features available on the selected architecture.

Additional test cases can initially be implemented as functions called in
sequence from `main()`. Registration, assertions, process isolation, and result
aggregation can be split into separate modules as needed.

Timing currently uses the existing `clock_gettime(0, ...)` interface, whose
underlying timestamps have microsecond resolution. The average in nanoseconds
is calculated from the elapsed time of the entire batch and includes loop
overhead; it does not imply nanosecond resolution for individual measurements.
`PASS` indicates that the measurement completed, not that performance met a
particular threshold.
