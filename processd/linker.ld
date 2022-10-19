OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
    . = 0x1000;
    INIT_TEXT_START = .;

    . = ALIGN(0x1000);
    .text :
    {
        *(.text)
        *(.rodata)
    }

    . = ALIGN(0x1000);
    .data :
    {
        INIT_DATA_START = .;
        *(.data)
    }

    . = ALIGN(0x1000);
    .bss :
    {
        INIT_BSS_START = .;
        *(.bss)
        *(COMMON)
    }
    . = ALIGN(0x1000);
}