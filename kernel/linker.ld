/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_limine_entry)

PHDRS
{
    text PT_LOAD FLAGS((1 << 0) | (1 << 2)) ;
    rodata PT_LOAD FLAGS((1 << 2)) ;
    data PT_LOAD FLAGS((1 << 1) | (1 << 2));
    eh_frame PT_LOAD FLAGS((1 << 2));
    dynamic PT_DYNAMIC FLAGS((1 << 1) | (1 << 2));
}

SECTIONS
{
    /* . = 0xFFFFFF0000000000; // 01777777760000000000000 */
    . = 0xFFFFFFFF80000000;

    /* Leave a symbol indicating the start of the kernel space */
    _kernel_start = .;

    /* Then let's place all the other traditional executable sections afterwards. */
    .text : {
        _text_start = .;
        *(.text .text.*)
        _text_end = .;
    } :text

    . += CONSTANT(MAXPAGESIZE);

    _rodata_start = .;
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    .init_array : {
        _init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        _init_array_end = .;
    } :rodata

    .fini_array : {
        _fini_array_start = .;
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        _fini_array_end = .;
    } :rodata
    _rodata_end = .;

    . += CONSTANT(MAXPAGESIZE);

    .data : {
        _data_start = .;
        *(.data .data.*)
        _data_end = .;
    } :data

    /* Dynamic section for relocations, both in its own PHDR and inside data PHDR */
    .dynamic : {
        *(.dynamic)
    } :data :dynamic
 
    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    } :data

    . += CONSTANT(MAXPAGESIZE);

    .eh_frame :
    {
        __eh_frame_start = .;
        KEEP(*(.eh_frame))
        __eh_frame_end = .;
    } :eh_frame

   .eh_frame_hdr :
    {
        _eh_frame_hdr_start = .;
        KEEP(*(.eh_frame_hdr))
        _eh_frame_hdr_end = .;
    } :eh_frame

    __eh_frame_hdr_start = SIZEOF(.eh_frame_hdr) > 0 ? ADDR(.eh_frame_hdr) : 0;
    __eh_frame_hdr_end = SIZEOF(.eh_frame_hdr) > 0 ? . : 0;

    .gcc_except_table : {
        _gcc_except_table_start = .;
        *(.gcc_except_table .gcc_except_table.*)
        _gcc_except_table_end = .;
    } :eh_frame

    _free_to_use = 0xFFFFFE8000000000;
    _free_after_kernel = ALIGN(4K);
}

