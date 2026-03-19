/* linker.ld — RISC-V virt bare metal kernel, entry: _start at 0x80000000 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

BASE_ADDRESS = 0xffffffc080000000;

MEMORY
{
  VIRT (rwx) : ORIGIN = 0xffffffc080000000 , LENGTH = 128M
  PHYSMEM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
  /* QEMU virt DRAM satrt address */
  . = BASE_ADDRESS;


  /* .text.entry（_start） */
  .text :
  {
    . = ALIGN(4);
    *(.text.entry)           /*  _start  */
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(0x1000);

    _divide = .;
  } > VIRT AT> PHYSMEM

  .data :
  {
    . = ALIGN(16);
    _data_start = .;
    *(.sdata .sdata.* .data .data.*)
    _data_end = .;
  }  > VIRT AT> PHYSMEM

  .bss :
  {
    . = ALIGN(16);
    _bss_start = .;
    *(.sbss .sbss.* .bss .bss.* COMMON)
    _bss_end = .;
  } > VIRT AT> PHYSMEM

  . = ALIGN(16);
  _stack_bottom = .;
  . = . + 0x4000;
  _stack_top = .;     /* alloc 16kb stack to kernel */

  . = ALIGN(0x1000);

  /* NOTE: High address mapping will cause address elevation*/
  _kernel_end = .;
}
