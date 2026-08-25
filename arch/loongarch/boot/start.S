    .section .text.entry
    .globl _start
    .type _start, @function

.equ DMW0, 0x180
.equ DMW1, 0x181

_start:
    # DMW0：将 0x8 开头的高地址映射到物理地址，允许 PLV0 访问，
    # MAT=1，作为内核代码和普通内存使用的一致可缓存窗口。
    li.d        $t2, 0x8000000000000011
    csrwr       $t2, DMW0
    # DMW1：将 0x9 开头的高地址映射到物理地址，允许 PLV0 访问，
    # MAT=0，作为 UART 等 MMIO 使用的强序非缓存窗口。
    li.d        $t2, 0x9000000000000001
    csrwr       $t2, DMW1
    ibar        0

    # QEMU 从物理地址别名进入，切换到 DMW0 的高地址可缓存映射继续执行。
    la.local    $t0, .Lhigh_entry
    li.d        $t2, 0x8000000000000000
    add.d       $t0, $t0, $t2
    jirl        $zero, $t0, 0

.Lhigh_entry:
    # 使用链接脚本预留的内核启动栈，并清零整个 BSS 区域。
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
