/* LoongArch virt kernel: DMW0 high address, loaded at physical 2 MiB. */
OUTPUT_ARCH(loongarch)
ENTRY(_start)

KERNEL_LMA = 0x0000000000200000;
KERNEL_VMA = 0x8000000000200000;

MEMORY
{
  VIRT (rwx) : ORIGIN = KERNEL_VMA, LENGTH = 126M
  PHYS (rwx) : ORIGIN = KERNEL_LMA, LENGTH = 126M
}

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = KERNEL_VMA;


  /* .text.entry（_start） */
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.text.entry))
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.srodata .srodata.*)
    . = ALIGN(0x1000);

    _divide = .;
  } > VIRT AT> PHYS :text

  .data :
  {
    . = ALIGN(16);
    _data_start = .;
    *(.sdata .sdata.* .data .data.*)
    . = ALIGN(0x1000);
    _data_end = .;
  }  > VIRT AT> PHYS :data

  .bss :
  {
    . = ALIGN(16);
    _bss_start = .;
    *(.sbss .sbss.* .bss .bss.* COMMON)
    _bss_end = .;

    . = ALIGN(16);
    _stack_bottom = .;
    . = . + 0x4000;
    _stack_top = .;     /* alloc 16kb stack to kernel */

  } > VIRT AT> PHYS :data


  . = ALIGN(0x1000);

  /* NOTE: High address mapping will cause address elevation*/
  _kernel_end = .;
}
