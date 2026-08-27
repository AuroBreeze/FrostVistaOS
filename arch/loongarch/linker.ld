OUTPUT_ARCH(loongarch)
ENTRY(_start)

KERNEL_VIRT_OFFSET = 0xffffffc000000000;

DMW0_BASE          = 0x8000000000000000;
BOOT_LMA           = 0x0000000000200000;
BOOT_VMA           = DMW0_BASE + BOOT_LMA;

BOOT_RESERVE       = 0x0000000000080000;
KERNEL_LMA         = BOOT_LMA + BOOT_RESERVE;
KERNEL_VMA         = KERNEL_LMA + KERNEL_VIRT_OFFSET;

PHDRS
{
  boot PT_LOAD FLAGS(7);
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  /*
   * 分页前运行的启动 trampoline。
   */
  .boot BOOT_VMA : AT(BOOT_LMA)
  {
    . = ALIGN(16);

    _boot_start = .;

    KEEP(*(.text.entry))
    KEEP(*(.text.boot))
    KEEP(*(.text.boot.*))
    KEEP(*(.data.boot))
    KEEP(*(.data.boot.*))
    KEEP(*(.rodata.boot))
    KEEP(*(.rodata.boot.*))

    . = ALIGN(8);
    _boot_bss_start = .;
    KEEP(*(.bss.boot))
    KEEP(*(.bss.boot.*))
    _boot_bss_end = .;

    . = ALIGN(16);
    _boot_stack_bottom = .;
    . = . + 0x4000;
    _boot_stack_top = .;

    . = ALIGN(0x1000);
    _boot_end = .;

    ASSERT(SIZEOF(.boot) <= BOOT_RESERVE,
       "LoongArch boot section exceeds BOOT_RESERVE");
  } :boot

  /*
   * 正式内核代码。
   *
   * VMA = LMA + KERNEL_VIRT_OFFSET
   */
  .text KERNEL_VMA : AT(KERNEL_LMA)
  {
    . = ALIGN(0x1000);

    _text_start = .;
    _text_start_pa = LOADADDR(.text);

    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.srodata .srodata.*)

    . = ALIGN(0x1000);

    _text_end = .;
    _text_end_pa = LOADADDR(.text) + SIZEOF(.text);
  } :text

  .data : AT(ALIGN(_text_end_pa, 0x1000))
  {
    . = ALIGN(0x1000);

    _data_start = .;
    _data_start_pa = LOADADDR(.data);

    *(.sdata .sdata.*)
    *(.data .data.*)
    *(.got .got.*)
    *(.got.plt .got.plt.*)

    . = ALIGN(0x1000);

    _data_end = .;
    _data_end_pa = LOADADDR(.data) + SIZEOF(.data);
  } :data

  .bss : AT(ALIGN(_data_end_pa, 0x1000))
  {
    . = ALIGN(0x1000);

    _bss_start = .;
    _bss_start_pa = LOADADDR(.bss);

    *(.sbss .sbss.*)
    *(.bss .bss.*)
    *(COMMON)

    _bss_end = .;

    . = ALIGN(16);
    _stack_bottom = .;
    . = . + 0x4000;
    _stack_top = .;


    . = ALIGN(0x1000);
    _bss_end_pa = LOADADDR(.bss) + SIZEOF(.bss);
  } :data

  . = ALIGN(0x1000);
  _kernel_end = .;
  _kernel_end_pa = _bss_end_pa;
}
