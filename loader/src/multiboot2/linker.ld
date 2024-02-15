/* Tell the linker that we want the symbol _start to be our entry point */
ENTRY(main)
 
SECTIONS
{
    /* Load in a fixed point */
    . = 0x8000;
    . = ALIGN(4096);
    _cpuinit_end = .;

    _start = .;
    _exec_start = .;
    .text : {
        . = ALIGN(4096);
        _cpuinit_start = .;
        *(.cpuinit)
        _cpuinit_end = .;
        *(.gdt) /* It's a bit hacky but (at least for now) putting GDT here makes the loader link.
            * GDT gets referenced by 16 bit code (hooray, 16, 32 and 64 bit code in one binary)
            * and if not put here, it gets pushed to the end of binary, breaking everything */
        *(.multiboot2)
        *(.text*)
    }
    .data : {
        *(.data*)
    }
    .rodata : {
        *(.rodata*)
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

