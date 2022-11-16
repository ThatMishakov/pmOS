/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(main)
 
SECTIONS
{
    /* Load in a fixed point */
    . = 0x4000;
    _cpuinit_start = .;
    .cpuinit : {
      *(.cpuinit)  
    }
    _cpuinit_end = ALIGN(4096);

    . = 4M;
    _start = .;
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
    
    _map_p_size = _map_p_end - _start;

    . = ALIGN(4096);
    _map_p_end = .;


    . = . + 256K; /* Reserve some space after executable */

    _exec_size = _end - _start;
    _exec_end = _end;
    _end = . ;
}

