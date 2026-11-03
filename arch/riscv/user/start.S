.section .text.entry, "ax", @progbits
.balign 4
.global _start
.type _start, @function

_start:
    # Initialize gp before C code can use relaxed global accesses.
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    # Preserve the argc/argv registers supplied by exec.
    tail __rime_start
.size _start, .-_start
