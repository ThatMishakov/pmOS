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

    /*  Push the pointer to the Multiboot information structure. */
    pushl   %ebx
    /*  Push the magic value. */
    pushl   %eax

    xchgw %bx, %bx
    /* Load GDT */
    pushl   $(temp_gdt_end - temp_gdt - 1) /* size */
    pushl   $temp_gdt /* base */
    call    loadGDT_32
    addl    $8, %esp

    xchgw %bx, %bx

    call main
    addl $8, %esp

    /* halt */
    cli
loop:   hlt
    jmp     loop

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

temp_gdt_64bit_code:
    .short 0x00 /* 64 bit data segment */
    .short 0x00
    .byte 0x00
    .byte 0x92
    .byte 0xa2
    .byte 0xc0
    .byte 0x00
temp_gdt_end:


