.section .text
.global __restore
.type __restore, @function

__restore:
    # Return from a signal handler through rt_sigreturn.
    li.w $a7, 139
    syscall 0
.size __restore, .-__restore
