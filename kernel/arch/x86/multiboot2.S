#define ASM_FILE
#include "multiboot2.h"

#ifdef __x86_64__
.equ KERNEL_OFFSET, (0xFFFFFFFF80000000 + 0x100000)
#else
.equ KERNEL_OFFSET, 0xc0000000 + 0x100000
#endif

.equ KERNEL_PHYS_BASE, 0x100000

.equ COM_PORT, 0x3f8

    .globl TEMP_AREA_SIZE
.equ TEMP_AREA_SIZE, 0x400000

.extern kernel_size
.set KERNEL_SIZE, kernel_size


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
    call multiboot_print_text
    jmp halt

1:
    leal (stack_end - rela_anchor)(%ebx), %esp

    // Align stack to 16, call multiboot_print_text with the booted text
    subl $12, %esp
    leal (booted_text - rela_anchor)(%ebx), %eax
    pushl %eax
    call multiboot_print_text
    addl $16, %esp

    // Set kernel phys base
    subl $8, %esp
    pushl $MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR
    pushl %esi
    call get_multiboot_tag
    addl $16, %esp
    cmpl $0, %eax
    je 1f
    movl 8(%eax), %edx
    movl %edx, (multiboot_kernel_base - rela_anchor)(%ebx)

    subl $KERNEL_PHYS_BASE, %edx
    movl %edx, (multiboot_kernel_phys_offset - rela_anchor)(%ebx)
1:

    call multiboot_map_kernel

    jmp multiboot_enter_kernel




get_multiboot_tag:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    subl $4, %esp


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
    addl $4, %esp
    popl %esi
    movl %ebp, %esp
    popl %ebp
    ret

    .globl multiboot_print_text
    .type multiboot_print_text, @function
multiboot_print_text:
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

    .globl print_hex32
    .type print_hex32, @function
print_hex32:
    movl $4, %ecx
    jmp 1f

    .type print_hex64, @function
print_hex64:
    movl $8, %ecx
1:
    
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi
    pushl %ebx
    subl $12, %esp

    call 9f
9:
    popl %ebx

    subl $32, %esp
    movl %esp, %edi
1:
    movzb 7(%ebp, %ecx), %esi
    movl %esi, %eax
    shrl $4, %eax
    movb (hex_lookup - 9b)(%ebx, %eax), %al
    movb %al, (%edi)
    movl %esi, %eax
    andl $0xf, %eax
    movb (hex_lookup - 9b)(%ebx, %eax), %al
    movb %al, 1(%edi)
    addl $2, %edi
    loop 1b

    movb $0, (%edi)

    movl %esp, %eax
    subl $24, %esp
    pushl %eax
    call multiboot_print_text

    movl -12(%ebp), %ebx
    movl -8(%ebp), %edi
    movl -4(%ebp), %esi 
    movl %ebp, %esp
    popl %ebp
    ret



    .globl clear_page32
    .type clear_page32, @function
clear_page32:
    pushl %edi
    movl $1024, %ecx
    movl 8(%esp), %edi
    xor %eax, %eax
    rep stosl
    popl %edi
    ret


    .globl multiboot_alloc_page_clear
    .type multiboot_alloc_page_clear, @function
multiboot_alloc_page_clear:
    pushl %ebp
    movl %esp, %ebp

    call 1f  
1:
    popl %edx
    movl (multiboot_temp_area_allocated - 1b)(%edx), %eax
    cmpl $TEMP_AREA_SIZE, %eax
    jae 2f

    addl $4096, (multiboot_temp_area_allocated - 1b)(%edx)
    leal (multiboot_temp_area - 1b)(%edx), %edx
    addl %edx, %eax

    movl %ebp, %esp
    popl %ebp
    ret

2: // Not enough memory
    subl $4, %esp
    leal (no_memory_text - 1b)(%edx), %eax
    pushl %eax
    call multiboot_print_text
    jmp halt



    // void pae_populate_pdpt(uint32_t pdpt_phys_addr, uint32_t virt_base)
    .globl pae_populate_pdpt
    .type pae_populate_pdpt, @function
pae_populate_pdpt:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    // Store pointer in %edi
    movl 8(%ebp), %edi

    // First 2MB needs to be mapped with 4KB mappings...
    cmpl $0x0, 12(%ebp)
    je 1f

    xorl %esi, %esi
    jmp 2f

1: // Map the first 2 MB with 4KB pages
    call multiboot_alloc_page_clear
    movl %eax, %ecx
    orl $0b11, %ecx
    movl %ecx, (%edi)

    xorl %esi, %esi
1:
    movl %esi, %ecx
    shl $12, %ecx
    orl $0b11, %ecx
    movl %ecx, (%eax, %esi, 8)
    incl %esi
    cmpl $512, %esi
    jb 1b

    movl $1, %esi
2:
    movl %esi, %eax
    shl $21, %eax
    addl 12(%ebp), %eax
    orl $0b10000011, %eax // Present, Writable, PS
    movl %eax, (%edi, %esi, 8) // Write the entry
    incl %esi
    cmpl $512, %esi
    jb 2b

    popl %edi
    popl %esi
    movl %ebp, %esp
    popl %ebp
    ret

// =================================================== .rodata ===================================================

    .rodata
hex_lookup: .string "0123456789abcdef"
booted_text: .string "pmOS kernel booted by multiboot2!\r\n"
bad_magic_text: .string "Multiboot2 bad magic, halting.\r\n"
no_memory_text: .string "Not enough memory available, halting.\r\n"
    .globl line_end_text
line_end_text: .string "\r\n"


// ================================================== .data ====================================================

    .data
    .align 4
    .globl multiboot_kernel_base
multiboot_kernel_base: .long KERNEL_PHYS_BASE

// =================================================== .bss =====================================================

    .bss
    .align 4
    .globl multiboot_temp_area_allocated
multiboot_temp_area_allocated: .long 0

    .globl multiboot_kernel_phys_offset
multiboot_kernel_phys_offset: .long 0

initialized_serial: .byte 0

// =================================================== .reclaimable =================================================
    .section .reclaimable.temp_area
    .globl multiboot_temp_area
multiboot_temp_area: // 3MB
    .align 4096
    .space TEMP_AREA_SIZE


// =================================================== .multiboot2 ===============================================

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
    .long   _multiboot_start - KERNEL_OFFSET + KERNEL_PHYS_BASE
entry_address_tag_end:

    .align 8
relocatable_header_tag_start:
    .short MULTIBOOT_HEADER_TAG_RELOCATABLE
    .short 0
    .long relocatable_header_tag_end - relocatable_header_tag_start
    // Min addr
    .long 0x100000 // Start at 1MB to leave lower memory available
    // Max addr
    .long 0xffffffff
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
    
    .align 8
    /* Terminate multiboot2 header */
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8   
multiboot_header_end: