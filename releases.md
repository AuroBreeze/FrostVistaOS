# Roadmap (v1.4 - Signals & Interactive Terminal Milestone)

v1.4 brings FrostVista its first real signal subsystem: the missing half of the process model. Signals give the kernel a mechanism for asynchronous process notification and forced termination, and give the shell a real Ctrl+C. The design follows the Linux RISC-V ABI so that musl-based user programs and the contest runner can use signals unchanged.

This milestone does not aim to implement full POSIX signal semantics, real-time signal queues, `sigaltstack`, `ptrace`, core dumps, per-thread signal masks, or job-control process groups. The goal is a clean, correct signal foundation: delivery, handlers, return, and interactive terminal behavior.

## Phase 1 - Kernel Signal Skeleton <!-- id: phase-1 -->

 - [x] **Process signal state** <!-- id: process-signal-state -->: `struct Process` gains pending/masked signal sets and a handler table; `fork` copies them.
 - [x] **Signal primitives** <!-- id: signal-primitives -->: `signal()` registration and `sigprocmask` basics in a new `kernel/core/signal.c`.
 - [x] **`kill` syscall** <!-- id: kill-syscall -->: locate a pid, set the pending bit, and wake a sleeping target.
 - [x] **`rt_sigpending`** <!-- id: rt-sigpending -->: query the pending signal mask from user space.

## Phase 2 - Delivery and Return <!-- id: phase-2 -->

 - [x] **Signal delivery checkpoint** <!-- id: signal-delivery-checkpoint -->: pending signals are checked before returning to user mode via `check_signal()` in `usertrapret`; default actions (terminate, ignore) are handled.
 - [x] **sigframe & handler entry** <!-- id: sigframe-handler-entry -->: build a RISC-V sigframe on the user stack and enter the handler with the signal number in `a0`.
 - [x] **`sigreturn`** <!-- id: sigreturn -->: restore the saved trapframe and signal mask from the sigframe and resume the interrupted instruction.
 - [x] **ABI alignment** <!-- id: riscv-musl-abi -->: use the RISC-V user stack alignment, handler `a0` argument, restorer return address, and `rt_sigreturn` syscall convention.

## Phase 3 - Interactive Terminal <!-- id: phase-3 -->

 - [x] **User-side wiring** <!-- id: user-signal-wiring -->: `kill()`/`rt_sigaction()`/`rt_sigprocmask()`/`rt_sigpending()` wrappers, terminal input ownership, and the `__restore` stub in the shared user runtime.
 - [x] **User sleep command** <!-- id: user-sleep-command -->: add the seconds-based `sleep` library wrapper and `/sleep` user application.
 - [x] **Ctrl+C in fvsh** <!-- id: ctrl-c-fvsh -->: the UART Ctrl+C path raises `SIGINT` for registered foreground children while the shell remains alive.
 - [x] **Faults become signals** <!-- id: faults-as-signals -->: unrecoverable user page faults raise `SIGSEGV` through normal signal delivery instead of panicking the kernel.

## Phase 4 - Regression Tests <!-- id: phase-4 -->

 - [x] **Signal lifecycle** <!-- id: signal-lifecycle -->: add a user-space SIGUSR1 raise, delivery, handler, and return-to-workflow round-trip test.
 - [x] **Ctrl+C shell behavior** <!-- id: ctrl-c-shell-test -->: inject a real 0x03 byte, interrupt a sleeping foreground child, verify status 130, and confirm the shell test process survives.
 - [x] **Fault-to-signal** <!-- id: fault-to-signal-test -->: an unmapped access terminates only the faulting child with status 139 while its parent and the kernel survive.

## Validation

 - [x] `python3 ./scripts/run_tests.py -t signal -T 20` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t fvsh_script -T 30` -> `PASS`
 - [x] `python3 ./scripts/run_tests.py -t fault_signal -T 20` -> `PASS_EXPECTED_LOG`
 - [x] Existing full suite still passes with signal delivery enabled.
