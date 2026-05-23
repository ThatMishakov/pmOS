#define ASM_FILE
#include "multiboot2.h"

#ifdef __x86_64__
.equ KERNEL_OFFSET, (0xFFFFFFFF80000000 + 0x100000)
#else
.equ KERNEL_OFFSET, 0
#endif

.equ COM_PORT, 0x3f8


    .code32
    .text
    .type _multiboot_start, @function
_multiboot_start:
    // Load stack:
    // ebx passes the Basic tags structure in %ebx. The second field in it is reserved, so it can be loaded to %esp,
    // followed by a call with popl, giving the relative kernel's relative address to then load the stack from data

    movl %ebx, %esi

    leal 8(%ebx), %esp
    call rela_anchor
rela_anchor:
    popl %ebx

    cmpl $MULTIBOOT2_BOOTLOADER_MAGIC, %eax
    je 1f
    // Bad magic
    subl $12, %esp
    leal (bad_magic_text - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    jmp halt

1:
    leal (stack_end - rela_anchor)(%ebx), %esp

    // Align stack to 16, call print_text with the booted text
    subl $12, %esp
    leal (booted_text - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    addl $16, %esp


    subl $8, %esp
    pushl $MULTIBOOT_TAG_TYPE_MMAP
    pushl %esi
    call get_multiboot_tag
    addl $16, %esp

    addl $12, %esp
    pushl %eax
    call print_memory_map
    addl $16, %esp

    jmp halt




get_multiboot_tag:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    subl $12, %esp


    movl 8(%ebp), %eax
    // Total size
    movl (%eax), %ecx
    // End in %ecx
    addl %eax, %ecx

    // Current pointer in %edx
    leal 8(%eax), %edx
1:
    movl (%edx), %eax
    cmpl %eax, 12(%ebp)
    je 2f

    movl 4(%edx), %eax
    addl $7, %eax
    andl $-8, %eax
    addl %eax, %edx
    cmpl %ecx, %edx
    jb 1b

    // Not found, return null
    xorl %eax, %eax
    jmp 1f

2:
    movl %edx, %eax
1:
    addl $12, %esp
    popl %esi
    movl %ebp, %esp
    popl %ebp
    ret

    
    .type print_text, @function
print_text:
    pushl %ebp
    movl %esp, %ebp

    cmpb $0, (initialized_serial - rela_anchor)(%ebx)
    jne 1f

    xorb %al, %al
    movw $(COM_PORT + 1), %dx
    outb %al, %dx
    movb $0x80, %al
    movw $(COM_PORT + 3), %dx
    outb %al, %dx
    movb $0x0C, %al
    movw $(COM_PORT + 0), %dx
    outb %al, %dx
    movb $0x00, %al
    movw $(COM_PORT + 1), %dx
    outb %al, %dx
    movb $0x03, %al
    movw $(COM_PORT + 3), %dx
    outb %al, %dx
    movb $0xC7, %al
    movw $(COM_PORT + 2), %dx
    outb %al, %dx
    movb $0x0f, %al
    movw $(COM_PORT + 4), %dx
    outb %al, %dx

    movb $1, (initialized_serial - rela_anchor)(%ebx) 
1:

    // Align the stack to 16 bytes
    subl $16, %esp

1:
    movl 8(%ebp), %eax
    movb (%eax), %al
    testb %al, %al
    je 2f
    movl %eax, (%esp)
    call com_print_char
    addl $1, 8(%ebp)
    jmp 1b
2:  

    movl %ebp, %esp
    popl %ebp
    ret

    .type com_print_char, @function
com_print_char:
    // Leaf function, don't bother with alignment and such

    movw $(COM_PORT + 5), %dx
    inb %dx, %al
    testb $0x20, %al
    jnz 1f
    pause
    jmp com_print_char
1:
    movb 4(%esp), %al
    movw $(COM_PORT + 0), %dx
    outb %al, %dx
    
    ret

    halt:
    cli
    hlt

    .type print_hex64, @function
print_hex64:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi
    subl $16, %esp

    subl 32, %esp
    movl %esp, %edi

    movl $8, %ecx
1:
    movzb 7(%ebp, %ecx), %esi
    movl %esi, %eax
    shrl $4, %eax
    movb (hex_lookup - rela_anchor)(%ebx, %eax), %al
    movb %al, (%edi)
    movl %esi, %eax
    andl $0xf, %eax
    movb (hex_lookup - rela_anchor)(%ebx, %eax), %al
    movb %al, 1(%edi)
    addl $2, %edi
    loop 1b

    movb $0, (%edi)

    movl %esp, %eax
    subl $24, %esp
    pushl %eax
    call print_text

    movl -8(%ebp), %edi
    movl -4(%ebp), %esi 
    movl %ebp, %esp
    popl %ebp
    ret

    .type print_memory_map, @function
print_memory_map:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi
    subl $8, %esp

    subl $12, %esp
    leal (memory_map_text - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text

    movl 8(%ebp), %eax // Memory map
    leal 16(%eax), %esi // Current entry
    movl 4(%eax), %edi // Size
    addl %eax, %edi // End of entries

1:
    cmpl %edi, %esi
    jge 1f

    leal (memory_map_entry_text - rela_anchor)(%ebx), %eax
    pushl %eax 
    call print_text

    subl $4, %esp
    movl (%esi), %eax
    movl 4(%esi), %edx
    pushl %edx
    pushl %eax
    call print_hex64
    addl $4, %esp

    leal (memory_map_entry_text2 - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    addl $4, %esp

    subl $4, %esp
    movl 8(%esi), %eax
    movl 12(%esi), %edx
    pushl %edx
    pushl %eax
    call print_hex64
    addl $4, %esp

    leal (memory_map_entry_text3 - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    addl $4, %esp

    subl $4, %esp
    pushl $0
    movl 16(%esi), %eax
    pushl %eax
    call print_hex64
    addl $4, %esp

    leal (memory_map_entry_text4 - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    addl $4, %esp

    movl 8(%ebp), %eax
    movl 8(%eax), %eax
    addl %eax, %esi
    jmp 1b

1:
    movl -8(%ebp), %edi
    movl  -4(%ebp), %esi
    movl %ebp, %esp
    popl %ebp
    retl

    .rodata
hex_lookup: .string "0123456789abcdef"
booted_text: .string "pmOS kernel booted by multiboot2!\r\n"
bad_magic_text: .string "Multiboot2 bad magic, halting.\r\n"
memory_map_text: .string "Memory map:\r\n"
memory_map_entry_text: .string "Base: 0x"
memory_map_entry_text2: .string ", Length: 0x"
memory_map_entry_text3: .string ", Type: 0x"
memory_map_entry_text4: .string "\r\n"

    .data
initialized_serial: .byte 0


    .section .multiboot2
    /*  Multiboot header. */
multiboot_header:
    .long   MULTIBOOT2_HEADER_MAGIC
    .long   MULTIBOOT_ARCHITECTURE_I386
    .long   multiboot_header_end - multiboot_header
    .long   0 - (MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header))

    /* Align modules to pages */
module_alligment_tag_start:
    .short  MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .short  0
    .long   module_alligment_tag_end - module_alligment_tag_start
module_alligment_tag_end: 

entry_address_tag_start:
    .short  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
    .short  0
    .long   entry_address_tag_end - entry_address_tag_start
    .long   _multiboot_start - KERNEL_OFFSET
entry_address_tag_end:

relocatable_header_tag_start:
    .short MULTIBOOT_HEADER_TAG_RELOCATABLE
    .short 0
    .long relocatable_header_tag_end - relocatable_header_tag_start
    // Min addr
    .long 0
    // Max addr
    .long -1
    // Align
    .long 4096
    // Preference
    .long 0
relocatable_header_tag_end:

framebuffer_tag_start:
    .short MULTIBOOT_HEADER_TAG_FRAMEBUFFER
    // Flags
    .short 0
    // Size
    .long framebuffer_tag_end - framebuffer_tag_start
    // Width
    .long 0 // No preference
    // Height
    .long 0 // No preference
    // Depth
    .long 0 // No preference
framebuffer_tag_end:
    
    /* Terminate multiboot2 header */
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8   
multiboot_header_end: