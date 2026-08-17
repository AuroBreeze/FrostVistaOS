# Roadmap (v1.4 - Signals & Interactive Terminal Milestone)

v1.4 brings FrostVista its first real signal subsystem: the missing half of the process model. Signals give the kernel a mechanism for asynchronous process notification and forced termination, and give the shell a real Ctrl+C. The design follows the Linux RISC-V ABI so that musl-based user programs and the contest runner can use signals unchanged.

This milestone does not aim to implement full POSIX signal semantics, real-time signal queues, `sigaltstack`, `ptrace`, core dumps, per-thread signal masks, or job-control process groups. The goal is a clean, correct signal foundation: delivery, handlers, return, and interactive terminal behavior.

## Phase 1 - Kernel Signal Skeleton <!-- id: phase-1 -->

 - [x] **Process signal state** <!-- id: process-signal-state -->: `struct Process` gains pending/masked signal sets and a handler table; `fork` copies them.
 - [x] **Signal primitives** <!-- id: signal-primitives -->: `signal()` registration and `sigprocmask` basics in a new `kernel/core/signal.c`.
 - [x] **`kill` syscall** <!-- id: kill-syscall -->: locate a pid, set the pending bit, and wake a sleeping target.
 - [ ] **`rt_sigpending`** <!-- id: rt-sigpending -->: query the pending signal mask (Phase 1 leftover, not yet implemented).

## Phase 2 - Delivery and Return <!-- id: phase-2 -->

 - [x] **Signal delivery checkpoint** <!-- id: signal-delivery-checkpoint -->: pending signals are checked before returning to user mode via `check_signal()` in `usertrapret`; default actions (terminate, ignore) are handled.
 - [x] **sigframe & handler entry** <!-- id: sigframe-handler-entry -->: build a RISC-V sigframe on the user stack and enter the handler with the signal number in `a0`.
 - [x] **`sigreturn`** <!-- id: sigreturn -->: restore the saved trapframe and signal mask from the sigframe and resume the interrupted instruction.
 - [x] **ABI alignment** <!-- id: riscv-musl-abi -->: use the RISC-V user stack alignment, handler `a0` argument, restorer return address, and `rt_sigreturn` syscall convention.

## Phase 3 - Interactive Terminal <!-- id: phase-3 -->

 - [x] **User-side wiring** <!-- id: user-signal-wiring -->: `kill()`/`rt_sigaction()`/`rt_sigprocmask()` wrappers and the `__restore` stub in the shared user runtime.
 - [ ] **Ctrl+C in fvsh** <!-- id: ctrl-c-fvsh -->: `collect_char` raises `SIGINT` on 0x03; the shell catches it and returns to a fresh prompt while child processes terminate.
 - [ ] **Faults become signals** <!-- id: faults-as-signals -->: page faults without a handler terminate the process instead of panicking the kernel.

## Phase 4 - Regression Tests <!-- id: phase-4 -->

 - [x] **Signal lifecycle** <!-- id: signal-lifecycle -->: add a user-space SIGUSR1 raise, delivery, handler, and return-to-workflow round-trip test.
 - [ ] **Ctrl+C shell behavior** <!-- id: ctrl-c-shell-test -->: interrupt a running command and confirm the shell survives.
 - [ ] **Fault-to-signal** <!-- id: fault-to-signal-test -->: SIGSEGV on an unmapped access kills only the faulting process.

## Validation

 - [ ] `python3 ./scripts/run_tests.py -t signal -T 20` -> `PASS`
 - [ ] `python3 ./scripts/run_tests.py -t fvsh_sigint -T 20` -> `PASS`
 - [ ] Existing full suite still passes with signal delivery enabled.
