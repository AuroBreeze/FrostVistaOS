    .section .text.entry
    .globl _start
    .type _start, @function

.equ DMW0, 0x180
.equ DMW1, 0x181

_start:
    li.d        $t2, 0x8000000000000011
    csrwr       $t2, DMW0
    li.d        $t2, 0x9000000000000001
    csrwr       $t2, DMW1
    ibar        0

    # QEMU enters through the physical alias; switch PC to cached DMW0.
    la.local    $t0, .Lhigh_entry
    li.d        $t2, 0x8000000000000000
    add.d       $t0, $t0, $t2
    jirl        $zero, $t0, 0

.Lhigh_entry:
    la.global   $sp, _stack_top
    la.global   $t0, _bss_start
    la.global   $t1, _bss_end

.Lclear_bss:
    bgeu        $t0, $t1, .Lbss_done
    st.d        $zero, $t0, 0
    addi.d      $t0, $t0, 8
    b           .Lclear_bss

.Lbss_done:
    bl          loong_early_boot

.Lhang:
    b           .Lhang

    .size _start, . - _start
