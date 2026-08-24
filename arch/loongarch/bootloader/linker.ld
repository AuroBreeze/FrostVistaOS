OUTPUT_ARCH(loongarch)
ENTRY(_start)

BOOT_BASE = 0x1c000000;

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x1c000000, LENGTH = 16M
    RAM   (rwx) : ORIGIN = 0x8000000000000000, LENGTH = 128M
}

SECTIONS
{
    . = BOOT_BASE;

    .text :
    {
        KEEP(*(.text.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > FLASH

    .kernel_blob ALIGN(16) :
    {
        _kernel_blob_start = .;
        KEEP(*(.data))
        _kernel_blob_end = .;
    } > FLASH

    .bss (NOLOAD) :
    {
        . = ORIGIN(RAM);
        . = ALIGN(16);
        _boot_bss_start = .;
        *(.bss .bss.* COMMON)
        _boot_bss_end = .;
        . = ALIGN(16);
        _boot_stack_bottom = .;
        . += 0x4000;
        _boot_stack_top = .;
    } > RAM

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.note.gnu.build-id)
        *(.comment)
    }
}
