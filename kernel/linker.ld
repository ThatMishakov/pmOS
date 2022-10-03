/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)
 
SECTIONS
{
    /* We wanna be placed in the higher half, 2MiB above 0 in physical memory. */
    /* Since we are going to use PIE, this is just the base load address, but the */
    /* bootloader will be able to relocate us as it sees fit. */
    . = 0xffffffff80200000;

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
}

