    .section .text
    .global kernelvec
    .align 12

    .type kernelvec, @function
kernelvec:
    b Eentry

    .size kernelvec, . - kernelvec
