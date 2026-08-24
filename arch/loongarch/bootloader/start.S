    .section .text.entry
    .globl _start
    .type _start, @function

_start:
    # Enable direct-mapped windows for cached RAM and MMIO.
    li.d        $t2, 0x8000000000000011
    csrwr       $t2, 0x180
    li.d        $t2, 0x9000000000000001
    csrwr       $t2, 0x181

    la.global   $sp, _boot_stack_top

    la.global   $t0, _boot_bss_start
    la.global   $t1, _boot_bss_end

1:
    bgeu        $t0, $t1, 2f
    st.d        $zero, $t0, 0
    addi.d      $t0, $t0, 8
    b           1b

2:
    bl          bootloader_main

3:
    b           3b

    .size _start, . - _start
