## v1.4.0 - Signals and Interactive Terminal

FrostVista adds Linux RISC-V signal delivery and connects it to terminal foreground-process ownership. Ctrl+C can now interrupt foreground commands without terminating the shell, unrecoverable user page faults become `SIGSEGV`, and sleeping processes block on timer wakeups instead of busy-yielding.

### Highlights

- **Signal lifecycle**: per-process pending and blocked masks, `kill`, `rt_sigaction`, `rt_sigprocmask`, `rt_sigpending`, handler entry, signal frames, and `rt_sigreturn` using the Linux RISC-V ABI.
- **Interactive Ctrl+C**: foreground terminal owners are registered by PID; UART byte `0x03` sends `SIGINT` to the registered commands while the shell remains alive.
- **Fault isolation**: unrecoverable user instruction/load/store page faults raise `SIGSEGV` through normal signal delivery instead of panicking the kernel; COW, lazy heap, and VMA faults remain recoverable.

### Additional

- **Build reliability**: user applications share one `restore.o` target, eliminating parallel writes under `make -j`; the test runner forces per-configuration kernel rebuilds and enables tmpfs for easyfs shell tests.
- **User sleep support**: timer-driven `nanosleep`, a seconds-based `sleep()` wrapper, and a `/sleep` user application.

### Validation

- `python3 ./scripts/run_tests.py -t signal -T 20` -> `PASS`
- `python3 ./scripts/run_tests.py -t fvsh_script -T 30` -> `PASS`
- `python3 ./scripts/run_tests.py -t fault_signal -T 20` -> `PASS_EXPECTED_LOG`
- EXT4 BusyBox runner proceeds past the repeated `echo ... >> /musl/basic/text.txt` sequence without an S-mode page-fault panic.
