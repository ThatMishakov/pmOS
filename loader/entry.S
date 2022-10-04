#define ASM_FILE        1
#include <multiboot2.h>
#include <asm.h>

#define GRUB_MULTIBOOT_ARCHITECTURE_I386 0
#define STACK_SIZE 0x4000

    .text
    .globl  start, _start
start:
_start:
    jmp     multiboot_entry
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




multiboot_entry:
    /*  Initialize the stack pointer. */
    movl    $(stack + STACK_SIZE), %esp

    /*  Reset EFLAGS. */
    pushl   $0
    popf

    /*  Push the pointer to the Multiboot information structure. */
    pushl   %ebx
    /*  Push the magic value. */
    pushl   %eax

    jmp main

    /* halt */
loop:   hlt
    jmp     loop

    /*  Our stack area. */
    .comm   stack, STACK_SIZE