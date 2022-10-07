#define ASM_FILE        1
#include <multiboot2.h>
#include <asm.h>

#define GRUB_MULTIBOOT_ARCHITECTURE_I386 0
#define STACK_SIZE 0x4000

    .section .multiboot2
    /*  Align 64 bits boundary. */
    .align  8    
    /*  Multiboot header. */
multiboot_header:
    /*  magic */
    .long   MULTIBOOT2_HEADER_MAGIC
    /*  ISA: i386 */
    .long   GRUB_MULTIBOOT_ARCHITECTURE_I386
    /*  Header length. */
    .long   multiboot_header_end - multiboot_header
    /*  checksum */
    .long   -(MULTIBOOT2_HEADER_MAGIC + GRUB_MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))
/* Align modules to pages */
module_alligment_tag_start:
    .short  MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .short  0
    .long   module_alligment_tag_end - module_alligment_tag_start
module_alligment_tag_end: 

    /* Terminate multiboot2 header */
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8   
multiboot_header_end:

    .code32
    .text
    .globl  _start
    .type _start, @function
_start:
    /*  Initialize the stack pointer. */
    movl    $(stack + STACK_SIZE), %esp

    /*  Reset EFLAGS. */
    pushl   $0
    popf

    /* Save multiboot structures */
    movl %ebx, multiboot_info_str
    movl %eax, multiboot_magic

    /* Load GDT */
    pushl   $(temp_gdt_end - temp_gdt - 1) /* size */
    pushl   $temp_gdt /* base */
    call    loadGDT_32
    addl    $8, %esp

    /* Init paging tables */
    call init_paging_tables

    /* Enable PAE */
    movl %cr0, %ebx
    andl ~(1 << 31), %ebx
    movl %ebx, %cr0

    /* Enable long mode */
    movl 0xc0000000, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    /* Load PML4 */
    movl $g_pml4, %eax
    movl %eax, %cr3

    /* Enable paging */
    orl (1 << 32) | (1 << 0), %ebx
    movl %ebx, %cr0

    /* Load 64 bit segment registers */
    movw 0x20, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* Far jump to main */
    ljmp   $0x18, $main


    /*  Our stack area. */
    .comm   stack, STACK_SIZE

    .align  4096
temp_gdt:
    .quad 0 /* Empty entry */

temp_gdt_32bit_code:
    .short 0xffff /* 32 bit code segment */
    .short 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xcf
    .byte 0x00

temp_gdt_32bit_data:
    .short 0xffff /* 32 bit data segment */
    .short 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xcf
    .byte 0x00

temp_gdt_64bit_code:
    .short 0x00 /* 64 bit code segment */
    .short 0x00
    .byte 0x00
    .byte 0x9a
    .byte 0xa2
    .byte 0xa0
    .byte 0x00

temp_gdt_64bit_data:
    .short 0x00 /* 64 bit data segment */
    .short 0x00
    .byte 0x00
    .byte 0x92
    .byte 0xa2
    .byte 0xc0
    .byte 0x00
temp_gdt_end:

    .text
    .align 4
    .code32
.type init_paging_tables, @function
init_paging_tables:
    /* Directly map 1 GB of memory */

    /* Add PDPT entry */
    movl $g_pdpt_e0, %eax
    andl $0xfffff000, %eax
    orl $0x000000403, %eax
    movl %eax, g_pml4
    movl $0, g_pml4+4

    /* Add PDE entry */
    movl $g_pd_e0, %eax
    andl $0xfffff000, %eax
    orl $0x000000403, %eax
    movl %eax, g_pdpt_e0
    movl $0, g_pdpt_e0+4

    /* Fill PD Table */
    movl 0, %ecx
p_loop_start:
    movl %ecx, %eax
    shll $21,  %eax
    orl 0x00000443, %eax
    movl %eax, g_pd_e0(,%eax,8)
    movl $0, g_pd_e0+4(,%eax,8)

    incl %ecx
    cmpl $512, %ecx
    jl p_loop_start

    ret

    .align 4096
g_pml4:
    .fill 4096

g_pdpt_e0:
    .fill 4096

    .section .bss
    .align 4096
    .comm g_pd_e0, 4096

