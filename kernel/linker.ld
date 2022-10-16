/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)
 
SECTIONS
{
    /* Load into upper half, leaving the last page for recursive mapping */
    . = 0xFFFFFF0000000000; /* 01777777760000000000000 */

    /* Leave a symbol indicating the start of the kernel space */
    _kernel_start = .;
    _free_to_use = 0xFFFFFE8000000000;

    /* Then let's place all the other traditional executable sections afterwards. */
    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }
 
    .data : {
        *(.data*)
    }
 
    .bss : {
        *(COMMON)
        *(.bss*)
    }
    _free_after_kernel = ALIGN(4K);

    //. = . + 2048M;
    _palloc_start = ALIGN(4K) + 0xb00b50000;
}

