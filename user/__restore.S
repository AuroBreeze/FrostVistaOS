.section .text
.global __restore
.type __restore, @function

__restore:
    li a7, 139
    ecall
