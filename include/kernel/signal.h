#ifndef __KERNEL_SIGNAL_H__
#define __KERNEL_SIGNAL_H__

#include "kernel/types.h"

#define NSIG 64

// Standard signal numbers (Linux asm-generic ABI).
#define SIGHUP 1     // Hangup
#define SIGINT 2     // Terminal interrupt (Ctrl+C)
#define SIGQUIT 3    // Terminal quit (Ctrl+\)
#define SIGILL 4     // Illegal instruction
#define SIGTRAP 5    // Trace/breakpoint trap
#define SIGABRT 6    // Abort
#define SIGBUS 7     // Bus error
#define SIGFPE 8     // Floating-point exception
#define SIGKILL 9    // Kill (cannot be blocked or ignored)
#define SIGUSR1 10   // User-defined signal 1
#define SIGSEGV 11   // Invalid memory reference
#define SIGUSR2 12   // User-defined signal 2
#define SIGPIPE 13   // Broken pipe
#define SIGALRM 14   // Timer/alarm
#define SIGTERM 15   // Termination
#define SIGSTKFLT 16 // Stack fault (unused on Linux)
#define SIGCHLD 17   // Child stopped or terminated
#define SIGCONT 18   // Continue if stopped
#define SIGSTOP 19   // Stop (cannot be blocked or ignored)
#define SIGTSTP 20   // Terminal stop (Ctrl+Z)
#define SIGTTIN 21   // Background process reading tty
#define SIGTTOU 22   // Background process writing tty
#define SIGURG 23    // Urgent condition on socket
#define SIGXCPU 24   // CPU time limit exceeded
#define SIGXFSZ 25   // File size limit exceeded
#define SIGVTALRM 26 // Virtual timer expired
#define SIGPROF 27   // Profiling timer expired
#define SIGWINCH 28  // Window size change
#define SIGIO 29     // I/O now possible
#define SIGPWR 30    // Power failure
#define SIGSYS 31    // Bad system call

// Default/ignore handler sentinels. Signal 0 is reserved for kill(pid, 0).
#define SIG_DFL ((uint64) 0) // Default behavior (terminate the process)
#define SIG_IGN ((uint64) 1) // Ignore the signal

// sigprocmask operation modes (Linux asm-generic ABI).
#define SIG_BLOCK 0   // Add the set to the blocked mask
#define SIG_UNBLOCK 1 // Remove the set from the blocked mask
#define SIG_SETMASK 2 // Replace the blocked mask with the set

// SA_RESTORER: the restorer field is valid. musl always sets this flag.
#define SA_RESTORER (1UL << 26) // 0x04000000

#define SIGMASK(sig) (1UL << ((sig) - 1))

// Field order must be handler, flags, restorer, mask -- matches musl's
// k_sigaction layout.
struct sigaction {
	uint64 handler; // User-space handler address; SIG_DFL/SIG_IGN are
			// special-cased
	uint64 flags;	// Only SA_RESTORER matters (SA_SIGINFO deferred)
	uint64
	    restorer; // User-space __restore stub address (ecall 139, Phase 3)
	uint64 mask;  // Extra signals to block while the handler runs
};

// Per-process signal state. All fields are protected by the owning
// struct Process's lock. pending/blocked are bitmasks: bit n is signal n.
struct sighand {
	uint64 sig_pending;		// Signals sent but not yet delivered
	uint64 sig_blocked;		// Signals blocked by sigprocmask
	struct sigaction actions[NSIG]; // Handler table
};

#endif
