OUTPUT_FORMAT("elf64-loongarch")
OUTPUT_ARCH(loongarch)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* PF_R | PF_X */
    data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
    . = 0x10000;

    .text : ALIGN(16)
    {
        *(.text.entry)
        *(.text)
        *(.text.*)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata)
        *(.rodata.*)
        *(.srodata)
        *(.srodata.*)
    } :text

    . = ALIGN(0x1000);

    .data : ALIGN(16)
    {
        *(.data)
        *(.data.*)

        *(.sdata)
        *(.sdata.*)

        *(.got)
        *(.got.*)
    } :data

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;

        *(.bss)
        *(.bss.*)

        *(.sbss)
        *(.sbss.*)

        *(COMMON)

        __bss_end = .;
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.interp)
        *(.dynamic)
        *(.dynsym)
        *(.dynstr)
        *(.gnu.hash)
        *(.hash)
    }

    ASSERT(ADDR(.text) >= 0x10000,
           "user text must not overlap the NULL guard region")
}
