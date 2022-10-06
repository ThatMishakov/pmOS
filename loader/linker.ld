/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(_start)
 
SECTIONS
{
    /* Load in a fixed point */
    . = 1M;

    .start = . ;
    .text : {
        *(.multiboot2)
        *(.text*)
    }

    _exec_start = 1234;
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
    _exec_start = .start;
    _exec_size = .end - .start;
    _exec_end = .end;
    .end = . ;
}

