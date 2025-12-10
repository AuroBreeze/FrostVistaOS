
ENTRY(_start)

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text.entry)
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
  }

  .data : {
    *(.data*)
  }

  .bss : {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    _bss_end = .;
  }

  . = ALIGN(16);
  _stack_top = . + 0x10000;
}



