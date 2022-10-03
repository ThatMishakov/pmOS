/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)
 
SECTIONS
{
    /* Load in a fixed point */
    . = 0x800000;

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

