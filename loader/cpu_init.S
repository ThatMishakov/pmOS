    .code16
    .section .cpuinit
    
    # Disable interrupts
    cli

    # Enable A20
    inb $0x92, %al
    orb $2, %al
    outb %al, $0x92

    # Load GDT
    lgdt cpuinit_gdt

    # Enter protected mode
    movl %cr0, %eax
    orb 1, %al
    movl %eax, %cr0

    jmp $(cpuinit_gdt_32bit_code - cpuinit_gdt),$protected_mode

    .code32
protected_mode:
    movw (cpuinit_gdt_32bit_data - cpuinit_gdt), %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    cmpb $0, nx_enabled
    jz nx_not_supported

    /* Enable NX */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 11), %eax
    wrmsr

nx_not_supported:
    /* Enable PAE and page size extension */
    movl %cr4, %eax
    orl $((1 << 5) | (1 << 4)), %eax
    movl %eax, %cr4

    /* Enable long mode */
    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr
    
    movl g_pml4, %eax
    movl %eax, %cr3

    /* Load 64 bit segment registers */
    movw $(cpuinit_gdt_64bit_data - cpuinit_gdt), %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    jmp $(cpuinit_gdt_64bit_data - cpuinit_gdt), $long_mode
long_mode:
    jmp *kernel_cpu_init


    .align 4096
cpuinit_gdt:
        .quad 0 /* Empty entry */
cpuinit_gdt_32bit_code:
    .short 0xffff /* 32 bit code segment */
    .short 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xcf
    .byte 0x00

cpuinit_gdt_32bit_data:
    .short 0xffff /* 32 bit data segment */
    .short 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xcf
    .byte 0x00

cpuinit_gdt_64bit_code:
    .short 0xffff /* 64 bit code segment */
    .short 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xaf
    .byte 0x00

cpuinit_gdt_64bit_data:
    .short 0xFFFF /* 64 bit data segment */
    .short 0x0000
    .byte 0x00
    .byte 0x92 /* ACESS BYTE */
    .byte 0xcf
    .byte 0x00
cpuinit_gdt_end:

