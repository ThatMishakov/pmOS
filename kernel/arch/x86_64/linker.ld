/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(kernel_main)

PHDRS
{
    text PT_LOAD FILEHDR PHDRS FLAGS((1 << 0) | (1 << 2)) ;
    rodata PT_LOAD FLAGS((1 << 2)) ;
    data PT_LOAD FLAGS((1 << 1) | (1 << 2));
    dynamic PT_DYNAMIC FLAGS((1 << 1) | (1 << 2));
}

SECTIONS
{
    /* . = 0xFFFFFF0000000000; // 01777777760000000000000 */
    kernel_offset = 0xFFFFFFFF80000000 + 0x100000;

    /* Leave a symbol indicating the start of the kernel space */
    _kernel_start = kernel_offset;
    KERNEL_PHYS_BASE = 0x100000;

    . = kernel_offset + SIZEOF_HEADERS;

    /* Then let's place all the other traditional executable sections afterwards. */
    .text : AT (ADDR (.text) - kernel_offset + KERNEL_PHYS_BASE) {
        *(.multiboot2)
        *(.text .text.*)
    } :text

    . += CONSTANT(MAXPAGESIZE);

    _rodata_start = .;
    .rodata : AT (ADDR (.rodata) - kernel_offset + KERNEL_PHYS_BASE) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } :rodata

    .init_array : AT (ADDR (.init_array) - kernel_offset + KERNEL_PHYS_BASE) {
        _init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        _init_array_end = .;
    } :rodata

    .fini_array : AT (ADDR (.fini_array) - kernel_offset + KERNEL_PHYS_BASE) {
        _fini_array_start = .;
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        _fini_array_end = .;
    } :rodata

    .eh_frame : AT (ADDR (.eh_frame) - kernel_offset + KERNEL_PHYS_BASE)
    {
        __eh_frame_start = .;
        KEEP(*(.eh_frame))
        __eh_frame_end = .;
    } :rodata

   .eh_frame_hdr : AT (ADDR (.eh_frame_hdr) - kernel_offset + KERNEL_PHYS_BASE)
    {
        KEEP(*(.eh_frame_hdr))
    
        __eh_frame_hdr_start = SIZEOF(.eh_frame_hdr) > 0 ? ADDR(.eh_frame_hdr) : 0;
        __eh_frame_hdr_end = SIZEOF(.eh_frame_hdr) > 0 ? . : 0;
    } :rodata

    .gcc_except_table : AT (ADDR (.gcc_except_table) - kernel_offset + KERNEL_PHYS_BASE)
    {
        _gcc_except_table_start = .;
        *(.gcc_except_table .gcc_except_table.*)
        _gcc_except_table_end = .;
    } :rodata

    . += CONSTANT(MAXPAGESIZE);

    .data : AT (ADDR (.data) - kernel_offset + KERNEL_PHYS_BASE) {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } :data

    /* Dynamic section for relocations, both in its own PHDR and inside data PHDR */
    .dynamic : AT (ADDR (.dynamic) - kernel_offset + KERNEL_PHYS_BASE) {
        *(.dynamic)
    } :data :dynamic
 
    .bss : AT (ADDR (.bss) - kernel_offset + KERNEL_PHYS_BASE) {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4096);
        _reclaimable_start = .;
        *(.reclaimable .reclaimable.*)
        _reclaimable_end = .;
        . = ALIGN(4096);
    } :data

    _kernel_end = .;
    kernel_end_aligned = ALIGN(4096);
    kernel_size = kernel_end_aligned - _kernel_start;
}

