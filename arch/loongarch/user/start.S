.section .text.entry, "ax", @progbits
.balign 4
.global _start
.type _start, @function

_start:
    # No register setup is needed; preserve argc/argv for the C runtime.
    b __rime_start
.size _start, .-_start
