    .section .text.entry
    .globl _start
    .type _start, @function

.equ DMW0, 0x180
.equ DMW1, 0x181

_start:
    /*
     * 配置 DMW0：普通内存
     */
    li.d        $t2, 0x8000000000000011
    csrwr       $t2, DMW0

    /*
     * 配置 DMW1：MMIO
     */
    li.d        $t2, 0x9000000000000001
    csrwr       $t2, DMW1

    ibar        0

    /*
     * 清空 boot 页表池和正式内核 BSS。
     *
     * _boot_bss_* 已经是 DMW0 下的启动地址；正式 .bss 的 *_pa
     * 符号是低端物理地址，必须先加上 DMW0_BASE 才能访问。这里不能
     * 使用正式高半区的 _bss_start/_bss_end，因为分页尚未开启。
     */
    la.global   $t0, _boot_bss_start
    la.global   $t1, _boot_bss_end

.Lclear_boot_bss:
    bgeu        $t0, $t1, .Lboot_bss_done
    st.d        $zero, $t0, 0
    addi.d      $t0, $t0, 8
    b           .Lclear_boot_bss

.Lboot_bss_done:
    la.global   $t0, _bss_start_pa
    la.global   $t1, _bss_end_pa
    li.d        $t2, 0x8000000000000000
    or          $t0, $t0, $t2
    or          $t1, $t1, $t2

.Lclear_bss:
    bgeu        $t0, $t1, .Lbss_done
    st.d        $zero, $t0, 0
    addi.d      $t0, $t0, 8
    b           .Lclear_bss

.Lbss_done:
    /*
     * 当前 QEMU 仍从低物理地址执行：
     *   PC = 0x000000000020....
     *
     * 先显式切到本地标签的 DMW0 别名：
     *   PC = 0x800000000020....
     */
    la.local    $t0, .Lboot_dmw_entry
    li.d        $t2, 0x8000000000000000
    or          $t0, $t0, $t2
    jirl        $zero, $t0, 0

.Lboot_dmw_entry:
    /*
     * 从这里开始，当前 PC 已经是 DMW0 地址。
     * 下面的 PC 相对 la.global 会自然得到 0x8000... 地址。
     */
    la.global   $sp, _boot_stack_top
    la.global   $t0, loongarch_bootstrap
    jirl        $zero, $t0, 0

.Lhang:
    b           .Lhang

    /*
     * 分页开启后仍然从 DMW0 进入这里。DMW0 在 mapped mode 下优先于
     * 页表，因此这个短跳板可以安全地执行；真正的内核入口和正式栈
     * 则必须已经由 boot_map_kernel() 建立高半区映射。
     */
    .section .text.boot
    .globl boot_jump_to_high
    .type boot_jump_to_high, @function

boot_jump_to_high:
    /* _stack_top 和 loong_early_boot 都是正式高半区 VMA。 */
    la.global   $sp, _stack_top
    la.global   $t0, loong_early_boot

    dbar        0
    ibar        0
    jirl        $zero, $t0, 0

.Lhigh_jump_hang:
    b           .Lhigh_jump_hang

    .size boot_jump_to_high, . - boot_jump_to_high
