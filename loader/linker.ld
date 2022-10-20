/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(main)
 
SECTIONS
{
    /* Load in a fixed point */

    .start = . ;

    . = ALIGN(4096);
    _exec_start = .;
    .text : {
        *(.multiboot2)
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
    _map_p_end = .map_p_end;
    _map_p_size = _map_p_end - .start;
    . = ALIGN(4096);
    .map_p_end = .;


    .paging : {
        KEEP(*(.paging))
    }
    _exec_size = .end - .start;
    _exec_end = .end;
    . = ALIGN(4096);
    .end = . ;
}

