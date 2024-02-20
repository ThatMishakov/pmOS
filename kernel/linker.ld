/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)

PHDRS
{
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
    eh_frame PT_LOAD;
    gcc_except_table PT_LOAD;
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
        *(.text*)
        _text_end = .;
    } :text

    . = ALIGN(4K);

    .rodata : {
        _rodata_start = .;
        *(.rodata*)
        _rodata_end = .;
    } :rodata

    . = ALIGN(4K);

    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } :data
 
    .bss : {
        _bss_start = .;
        *(COMMON)
        *(.bss*)
        _bss_end = .;
    } :bss

    . = ALIGN(4K);

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
        *(.gcc_except_table*)
        _gcc_except_table_end = .;
    } :gcc_except_table

    _free_to_use = 0xFFFFFE8000000000;
    _free_after_kernel = ALIGN(4K);

    //. = . + 2048M;
    //_palloc_start = ALIGN(4K) + 0xb00b50000;
}

