/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)
 
SECTIONS
{
    /* Load into upper half, leaving the last page for recursive mapping */
    /* . = 0xFFFFFF0000000000; // 01777777760000000000000 */
    . = 0xFFFFFFFF80000000;

    /* Leave a symbol indicating the start of the kernel space */
    _kernel_start = .;

    /* Then let's place all the other traditional executable sections afterwards. */
    . = ALIGN(4K);
    .text : {
        *(.text*)
        . = ALIGN(4K);
    }

    .rodata : {
        *(.rodata*)
        . = ALIGN(4K);
    }

    .data : {
        *(.data*)
        . = ALIGN(4K);
    }
 
    .bss : {
        *(COMMON)
        *(.bss*)
        . = ALIGN(4K);
    }

    .eh_frame_hdr : {
        PROVIDE (__eh_frame_hdr_start = .);
        *(.eh_frame_hdr*)
        PROVIDE (__eh_frame_hdr_end = .);
        . = ALIGN(4K);
    }

    .eh_frame : {
        PROVIDE (__eh_frame_start = .);
        *(.eh_frame*)
        PROVIDE (__eh_frame_end = .);
        . = ALIGN(4K);
    }

    .gcc_except_table : {
        *(.gcc_except_table*)
        . = ALIGN(4K);
    }

    _free_to_use = 0xFFFFFE8000000000;
    _free_after_kernel = ALIGN(4K);

    //. = . + 2048M;
    //_palloc_start = ALIGN(4K) + 0xb00b50000;
}

