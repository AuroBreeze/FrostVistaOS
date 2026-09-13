    .section .text.entry
    .globl _start
    .type _start, @function

.equ DMW0, 0x180
.equ DMW1, 0x181

_start:
    /*
     * Configure DMW0 for normal memory.
     */
    li.d        $t2, 0x8000000000000011
    csrwr       $t2, DMW0

    /*
     * Configure DMW1 for MMIO.
     */
    li.d        $t2, 0x9000000000000001
    csrwr       $t2, DMW1

    ibar        0

    /*
     * Clear the boot page-table pool and the final kernel BSS.
     *
     * _boot_bss_* already names boot addresses in DMW0. The *_pa symbols for
     * the final .bss are low physical addresses and require DMW0_BASE before
     * they can be accessed. The final high-half _bss_start/_bss_end symbols
     * cannot be used here because paging is not enabled yet.
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
     * QEMU is still executing from a low physical address:
     *   PC = 0x000000000020....
     *
     * Explicitly switch to the local label's DMW0 alias first:
     *   PC = 0x800000000020....
     */
    la.local    $t0, .Lboot_dmw_entry
    li.d        $t2, 0x8000000000000000
    or          $t0, $t0, $t2
    jirl        $zero, $t0, 0

.Lboot_dmw_entry:
    /*
     * From this point onward, the current PC is a DMW0 address. The
     * PC-relative la.global instructions below naturally produce 0x8000...
     * addresses.
     */
    la.global   $sp, _boot_stack_top
    la.global   $t0, loongarch_bootstrap
    jirl        $zero, $t0, 0

.Lhang:
    b           .Lhang

    /*
     * Execution still enters here through DMW0 after paging is enabled. DMW0
     * takes precedence over page tables in mapped mode, so this short
     * trampoline is safe. boot_map_kernel() must already have created
     * high-half mappings for the real kernel entry point and final stack.
     */
    .section .text.boot
    .globl boot_jump_to_high
    .type boot_jump_to_high, @function

boot_jump_to_high:
    /* _stack_top and loong_early_boot are both final high-half VMAs. */
    la.global   $sp, _stack_top
    la.global   $t0, loong_early_boot

    dbar        0
    ibar        0
    jirl        $zero, $t0, 0

.Lhigh_jump_hang:
    b           .Lhigh_jump_hang

    .size boot_jump_to_high, . - boot_jump_to_high
