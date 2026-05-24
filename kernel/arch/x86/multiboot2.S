#define ASM_FILE
#include "multiboot2.h"

#ifdef __x86_64__
.equ KERNEL_OFFSET, (0xFFFFFFFF80000000 + 0x100000)
#else
.equ KERNEL_OFFSET, 0
#endif

.equ COM_PORT, 0x3f8

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

    // Set kernel phys base
    subl $8, %esp
    pushl $MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR
    pushl %esi
    call get_multiboot_tag
    addl $16, %esp
    cmpl $0, %eax
    je 1f
    leal (multiboot_kernel_base - rela_anchor)(%ebx), %ecx
    movl %eax, (%ecx)
1:

    subl $12, %esp
    pushl %esi
    call find_largest_available_region
    addl $16, %esp

    subl $12, %esp
    leal (temp_area_text_1 - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    addl $4, %esp
    pushl (multiboot_temp_area - rela_anchor)(%ebx)
    call print_hex32
    addl $4, %esp
    leal (temp_area_text_2 - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
    addl $4, %esp
    pushl (multiboot_temp_area_size - rela_anchor)(%ebx)
    call print_hex32
    addl $4, %esp
    leal (end - rela_anchor)(%ebx), %eax
    pushl %eax
    call print_text
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
    subl $16, %esp

    subl 32, %esp
    movl %esp, %edi
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
    jae 1f

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

    // Params:
    // uint32_t multiboot_info_addr
    .type find_largest_available_region, @function
find_largest_available_region:
    pushl %ebp
    movl %esp, %ebp
    subl $16, %esp
    movl %esi, -4(%ebp)
    movl %edi, -8(%ebp)

    // Find the memory map tag
    subl $8, %esp
    movl 8(%ebp), %eax
    pushl $MULTIBOOT_TAG_TYPE_MMAP
    pushl %eax
    call get_multiboot_tag
    addl $16, %esp

    movl %eax, -12(%ebp) // Memory map tag in -12(%ebp)
    movl 4(%eax), %edi // Size of memory map
    addl %eax, %edi 
    movl %edi, -16(%ebp) // End of memory map entries in -16(%ebp)
    leal 16(%eax), %esi // Current entry

    // Here, %esi stores the current offset in the memory map.
    // %edi is free as a scratch register

    // while (entry < end)
2:
    cmpl -16(%ebp), %esi
    jae 1f

    movl 16(%esi), %eax // Type
    cmpl $1, %eax // MULTIBOOT_MEMORY_AVAILABLE
    jne 3f

    cmpl $0, 4(%esi) // base_addr high
    jne 3f // Above 4GB (not usable in protected mode without paging so can't be used)

    cmpl $0, 12(%esi) // Check if the entry size is above 4GB (which would always overflow the entry)
    je 5f

    movl (%esi), %ecx // Base addr low
    movl %ecx, %edx
    neg %edx // size = -base_addr
    jmp 4f

5: // Size less than 4GB
   // Check if base + size is above 4GB, reduce size to fit if it is
   movl (%esi), %ecx // Base addr low
   movl 8(%esi), %edx // Size low

   // Check if base + size is above 4GB
   movl %ecx, %edi
   addl %edx, %edi
   jnc 4f // Not above 4GB

   // Above 4GB, reduce size
   movl %ecx, %edx
   neg %edx

4: // base + size below 4GB. Base in %ecx, size in %edx

    // Multiboot2 doesn't guarantee the alignment of the entries...
    movl %ecx, %eax
    andl $0xfff, %eax
    jz 6f

    negl %eax
    addl $4096, %eax
    cmpl %eax, %edx
    jb 3f // Size smaller than the rounded up value

    addl %eax, %ecx // Align base up to PAGE_SIZE
    subl %eax, %edx

6: // Base is aligned here
    andl $0xfffff000, %edx // Align size down to PAGE_SIZE

    subl $4, %esp // Alignment
    pushl %edx // region_size
    pushl %ecx // region_base
    pushl 8(%ebp) // multiboot_info_addr

    call check_memory_region
    addl $16, %esp

3:
    movl -12(%ebp), %eax // Memory map tag
    movl 8(%eax), %eax // Entry size
    addl %eax, %esi // Next entry

    jmp 2b
1:

    movl -8(%ebp), %edi
    movl -4(%ebp), %esi
    movl %ebp, %esp
    popl %ebp
    ret

    // void check_memory_region(void *multiboot_info_addr, uint32_t region_base, uint32_t region_size)
    .type check_memory_region, @function
check_memory_region:
    pushl %ebp
    movl %esp, %ebp
    subl $16, %esp
    movl %esi, -4(%ebp)
    movl %edi, -8(%ebp)

    // Multiboot2 info
    movl 8(%ebp), %ecx
    movl (%ecx), %edx // Size
    movl %ecx, %eax
    andl $0xfff, %eax
    subl %eax, %ecx // Align begining of multiboot info down to PAGE_SIZE
    addl %eax, %edx
    addl $0xfff, %edx
    andl $0xfffff000, %edx // Align size up to PAGE_SIZE

    subl $4, %esp
    pushl %edx // reserved_size
    pushl %ecx // reserved_base
    leal 12(%ebp), %eax
    pushl %eax // arg
    call adjust_region
    addl $16, %esp

    // Kernel base
    subl $4, %esp
    pushl $KERNEL_SIZE
    pushl (multiboot_kernel_base - rela_anchor)(%ebx)
    leal 12(%ebp), %eax
    pushl %eax
    call adjust_region
    addl $16, %esp

    // Modules...
    movl 8(%ebp), %esi
    movl (%esi), %edi // Size of the multiboot info
    addl %esi, %edi // End of multiboot info
    leal 8(%esi), %esi // Current tag
1: // while (%esi < %edi)
    cmpl %edi, %esi
    jae 2f

    movl (%esi), %eax
    cmpl $MULTIBOOT_TAG_TYPE_MODULE, %eax
    jne 3f

    subl $4, %esp
    movl 12(%esi), %eax
    // Align the end to PAGE_SIZE
    addl $4095, %eax
    andl $-4096, %eax
    subl 8(%esi), %eax // Subtract the base address to get the size
    pushl %eax // region_size
    // The start of the module should be aligned to PAGE_SIZE
    pushl 8(%esi) // region_base
    leal 12(%ebp), %eax
    pushl %eax // arg
    call adjust_region
    addl $16, %esp

3:  // %esi += size of the tag + alignment
    movl 4(%esi), %eax
    addl $7, %eax
    andl $-8, %eax
    addl %eax, %esi
    jmp 1b
2: // End of loop

    movl 16(%ebp), %eax
    cmpl (multiboot_temp_area_size - rela_anchor)(%ebx), %eax
    jbe 4f

    movl 12(%ebp), %edx
    movl %edx, (multiboot_temp_area - rela_anchor)(%ebx)
    movl %eax, (multiboot_temp_area_size - rela_anchor)(%ebx)

4:

    movl -4(%ebp), %esi
    movl -8(%ebp), %edi
    movl %ebp, %esp
    popl %ebp
    ret

    // struct arg {
    //   uint32_t region_base;
    //   uint32_t region_size;   
    // };
    // void adjust_region(struct arg *arg, uint32_t reserved_base, uint32_t reserved_size)
    .type adjust_region, @function
adjust_region:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %eax // arg
    cmpl $0, 4(%eax) // Check if region_size is 0
    je 1f // If it is, skip
    cmpl $0, 16(%ebp) // Check if reserved_size is 0
    je 1f // If it is, skip

    movl (%eax), %ecx // %ecx = region_base
    cmpl %ecx, 12(%ebp) // Compare region_base with reserved_base
    ja 1f

    // Check if the region overlaps with the reserved area, if it does, move the region base up and reduce the region size
    subl 12(%ebp), %ecx // %ecx = region_base - reserved_base
    cmpl 16(%ebp), %ecx // Compare with reserved_size
    jae adjust_region_end // If reserved_size <= region_base - reserved_base, there is no overlap

    movl 16(%ebp), %edx // %edx = reserved_size
    subl %ecx, %edx // %edx = reserved_size - (region_base - reserved_base)

    cmpl %edx, 4(%eax) // Compare with region_size
    jb 2f // Reserved region is larger than the available region

    addl %edx, (%eax) // region_base += %edx
    subl %edx, 4(%eax) // region_size -= %edx
    jmp adjust_region_end // Done

2:
    movl 4(%eax), %edx // %edx = region_size
    addl %edx, (%eax) // region_base += region_size
    movl $0, 4(%eax) // region_size = 0
    jmp adjust_region_end // Done

1: // region_base < reserved_base
    movl 12(%ebp), %ecx // %ecx = reserved_base
    subl (%eax), %ecx // %ecx = reserved_base - region_base; in other words, free space before the reserved region
    cmpl 4(%eax), %ecx // Compare with region size
    jae adjust_region_end // If reserved_base - region_base >= region_size, there is no overlap

    movl 16(%ebp), %edx // %edx = reserved_size
    addl %ecx, %edx // %edx = reserved_size + (reserved_base - region_base); in other words,
                    // offset to the end of the reserved region relative to the region base
    cmpl %edx, 4(%eax) // Compare with region_size
    ja 2f // Jump if the the reserved region is in the middle

    movl 12(%ebp), %ecx // %ecx = reserved_base
    subl (%eax), %ecx // %ecx = reserved_base - region_base
    movl %ecx, 4(%eax) // region_size = reserved_base - region_base 
    jmp adjust_region_end // Done

2:
    // Chose the larger half and use that as the new region
    subl 4(%eax), %edx // %edx = -(end of reserved area - region_base)
    negl %edx // %edx = end of reserved area - region_base; free space after the reserved region
    cmpl %ecx, %edx // src = lower half size, dst = higher half size
    ja 1f // Jump if lower half is larger

    movl 4(%eax), %ecx // %ecx = region_size
    subl %edx, %ecx // Size diff
    addl %ecx, (%eax) // region_base += size diff
    movl %edx, 4(%eax) // region_size = size of higher half
    jmp adjust_region_end    

1:
    movl %ecx, 4(%eax) // region_size = size of lower half

adjust_region_end: // end
    movl %ebp, %esp
    popl %ebp
    ret

// =================================================== .rodata ===================================================

    .rodata
hex_lookup: .string "0123456789abcdef"
booted_text: .string "pmOS kernel booted by multiboot2!\r\n"
bad_magic_text: .string "Multiboot2 bad magic, halting.\r\n"
memory_map_text: .string "Memory map:\r\n"
memory_map_entry_text: .string "Base: 0x"
memory_map_entry_text2: .string ", Length: 0x"
memory_map_entry_text3: .string ", Type: 0x"
memory_map_entry_text4: .string "\r\n"
temp_area_text_1: .string "Allocating memory from 0x"
temp_area_text_2: .string " size 0x"
end: .string "\r\n"


// =================================================== .data =====================================================

    .data
    .align 4
multiboot_temp_area: .long 0
multiboot_temp_area_size: .long 0
    .globl multiboot_kernel_base
multiboot_kernel_base: .long 0

initialized_serial: .byte 0


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