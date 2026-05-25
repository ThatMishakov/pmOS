// =================== 32 bit code ===================
.extern multiboot_print_text
.extern multiboot_alloc_page
.extern multiboot_alloc_page_clear
.extern __ehdr_start
.extern __ehdr_end

.extern multiboot_kernel_phys_offset

.equ CPUID_EXTENDED_MAX, 0x80000000
.equ CPUID_EXTENDED_FEATURES, 0x80000001

.equ CPUID_NX_BIT, 20
.equ CPUID_LONG_MODE_BIT, 29
.equ CPUID_5LVL_PAGING_BIT, 27

.equ NX_BIT, 0
.equ PAGING_5LVL_BIT, 1

.equ MAP_EXECUTABLE, 1
.equ MAP_WRITABLE, 2

.equ pheader_size, 64
.equ PT_LOAD, 1

    .text
    .code32
    .global multiboot_map_kernel
    .type multiboot_map_kernel, @function
multiboot_map_kernel:
    pushl %ebp
    movl %esp, %ebp
    subl $24, %esp
    movl %ebx, -4(%ebp)
    movl %esi, -8(%ebp)
    movl %edi, -12(%ebp)

    call 9f
9:
    popl %edi

    // Detect CPUID
    pushfd // Save EFLAGS
    pushfd // Save eflags again
    xorl $0x200000, (%esp) // Toggle ID bit in EFLAGS
    popfd // Restore EFLAGS
    pushfd // Get EFLAGS again
    popl %eax // Store EFLAGS in EAX
    xorl (%esp), %eax // Check if ID bit changed
    popfd // Restore original EFLAGS
    andl $0x200000, %eax // Check if ID bit is set
    jz unsupported_cpu // There's no CPUID, CPU is not supported

    // Check max leaf
    movl $CPUID_EXTENDED_MAX, %eax
    cpuid
    cmpl $CPUID_EXTENDED_FEATURES, %eax
    jb unsupported_cpu // CPUID does not support extended functions, CPU is not supported

    // Check extended features
    movl $CPUID_EXTENDED_FEATURES, %eax
    cpuid
    btl $CPUID_LONG_MODE_BIT, %edx // Check if long mode is supported
    jnc unsupported_cpu // Long mode not supported, CPU is not supported

    btl $CPUID_5LVL_PAGING_BIT, %edx // Check if 5-level paging is supported
    jnc 1f // 5-level paging not supported
    bts $PAGING_5LVL_BIT, (cpu_features - 9b)(%edi) // Set 5-level paging bit in features
1:

    // Check NX
    btl $CPUID_NX_BIT, %edx // Check if NX bit is supported
    jnc 2f // NX bit not supported
    bts $NX_BIT, (cpu_features - 9b)(%edi) // Set NX bit in features
2:

    // Alloc cr3
    call multiboot_alloc_page_clear
    movl %eax, multiboot_cr3 - 9b(%edi)

    // Map the kernel
    leal (__ehdr_start - 9b)(%edi), %ebx
    // %esi = phnum
    movl 88(%ebx), %esi
    // %esi = phnum * 64
    shl $6, %esi
    // %ebx = current address of the first program header
    addl 48(%ebx), %ebx
    // %esi = address of the end of the program headers
    addl %ebx, %esi
1:
    cmpl %esi, %ebx
    jae 2f

    // Check if loadable
    // The entries are 64 bits long, but the upper half should be 0 (?)
    cmpl $PT_LOAD, (%ebx) // Check if it's a loadable segment
    jne 3f // Not loadable, skip

    subl $12, %esp
    pushl 8(%ebx) // flags
    andl $3, (%esp) // Keep writable and executable bits

    movl 32(%ebx), %eax // paddr
    addl (multiboot_kernel_phys_offset - 9b)(%edi), %eax // Offset addr by what it was moved by the bootloader
    pushl %eax // phys addr

    pushl 48(%ebx) // memsz
    pushl 28(%ebx) // p_vaddr high
    pushl 24(%ebx) // p_vaddr low
    call bootstrap_map
    addl $32, %esp

3:
    addl $pheader_size, %ebx
    jmp 1b
2:

    movl -12(%ebp), %edi
    movl -8(%ebp), %esi
    movl -4(%ebp), %ebx
    movl %ebp, %esp
    popl %ebp
    ret

    .type unsupported_cpu, @function
unsupported_cpu:
    pushl %ebp
    call 1f
1:
    popl %ebp

    leal (unsupported_cpu_msg - 9b)(%ebx), %eax
    pushl %eax
    call multiboot_print_text
    addl $16, %esp
 
    jmp halt


.type halt, @function
halt:
    cli
    hlt

    // void bootstrap_map(uint64_t virt_addr, uint32_t size, uint32_t phys_addr, uint32_t flags)
    .type bootstrap_map, @function
bootstrap_map:
    pushl %ebp
    movl %esp, %ebp
    subl $8, %esp
    movl %esi, -4(%ebp)
    movl %edi, -8(%ebp)

    // 1. Round the virt/phys addresses down and size up to page boundaries
    movl 8(%ebp), %eax // virt_addr low
    andl $0xfff, %eax // Get offset within page
    subl %eax, 8(%ebp) // Round virt_addr down to page boundary
    subl %eax, 20(%ebp) // Round phys_addr down to page boundary
    addl %eax, 16(%ebp) // Increase size by offset to account for rounding down
    addl $0xfff, 16(%ebp) // Round size up to
    andl $0xfffff000, 16(%ebp)

    // 2. Map the pages
    // This is not the optimal way to do it, but since there aren't that many pages to map, it should be fine
    xorl %esi, %esi // unsigned offset = 0;
1:
    cmpl %esi, 16(%ebp)
    jbe 2f // Jump when size <= offset

    pushl 24(%ebp) // flags
    pushl 20(%ebp) // phys addr
    addl %esi, (%esp) // phys addr + offset
    pushl 12(%ebp) // virt addr high
    pushl 8(%ebp) // virt addr low
    addl %esi, (%esp) // virt addr + offset
    adcl $0, 4(%esp) // Handle carry for high part of virt addr
    call map_page
    addl $16, %esp

    addl $0x1000, %esi // offset += 4096
    jmp 1b

2: // End

    movl -8(%ebp), %edi
    movl -4(%ebp), %esi
    movl %ebp, %esp
    popl %ebp
    ret

    // void map_page(uint64_t virt_addr, uint32_t phys_addr, uint32_t flags)
    .type map_page, @function
map_page:
    pushl %ebp
    movl %esp, %ebp
    subl $24, %esp
    movl %esi, -4(%ebp)
    movl %edi, -8(%ebp)
    movl %ebx, -12(%ebp)

    call 1f
9:
    popl %ebx
    movl (multiboot_cr3 - 9b)(%ebx), %esi // %esi = current page table base

    btl $PAGING_5LVL_BIT, (cpu_features - 9b)(%ebx)
    jnc 1f // CPU does not support 5-level paging, skip

    movl 12(%ebp), %edi // virt_addr high
    shr $16, %edi // Get PML5 index
    andl $0x1ff, %edi // Mask to 9 bits

    movl (%esi, %edi, 8), %eax // Get PML5 entry
    testl $1, %eax // Check if present
    jz 2f // Not present
    andl $0xfffff000, %eax // Get address of next level page table
    movl %eax, %esi // Move to next level
    jmp 1f

2:
    call multiboot_alloc_page_clear
    orl $0b11, %eax // Set present and writable bits
    movl %eax, (%esi, %edi, 8) // Write PML5 entry
    movl %eax, %esi // Move to next level

1: // PML4
    movl 12(%ebp), %edi // virt_addr high
    shr $7, %edi // Get PML4 index
    andl $0x1ff, %edi // Mask to 9 bits
    movl (%esi, %edi, 8), %eax // Get PML4 entry
    testl $1, %eax // Check if present
    jz 2f // Not present
    andl $0xfffff000, %eax // Get address of next level page table
    movl %eax, %esi // Move to next level
    jmp 1b
2:
    call multiboot_alloc_page_clear
    orl $0b11, %eax // Set present and writable bits
    movl %eax, (%esi, %edi, 8) // Write PML5 entry
    movl %eax, %esi // Move to next level


1: // PDPT (this one is anoying)
    movl 8(%ebp), %edi // virt_addr low
    shr $30, %edi // Get PDPT index
    andl $0x3, %edi // Mask to 2 bits
    movl 12(%ebp), %eax // virt_addr high
    shl $2, %eax
    andl $0x1fc, %eax
    orl %eax, %edi // get full index

    movl (%esi, %edi, 8), %eax // Get PDPT entry
    testl $1, %eax // Check if present
    jz 2f // Not present
    andl $0xfffff000, %eax // Get address of next level page table
    movl %eax, %esi // Move to next level
    jmp 1b
2:
    call multiboot_alloc_page_clear
    orl $0b11, %eax // Set present and writable bits
    movl %eax, (%esi, %edi, 8) // Write PDPT
    movl %eax, %esi // Move to next level

1: // PD
    movl 8(%ebp), %edi // virt_addr low
    shr $21, %edi // Get PD index
    andl $0x1ff, %edi // Mask to 9 bits

    movl (%esi, %edi, 8), %eax // Get PD entry
    testl $1, %eax // Check if present
    jz 2f // Not present
    andl $0xfffff000, %eax // Get address of next level page table
    movl %eax, %esi // Move to next level
    jmp 1b
2:
    call multiboot_alloc_page_clear
    orl $0b11, %eax // Set present and writable bits
    movl %eax, (%esi, %edi, 8) // Write PD entry
    movl %eax, %esi // Move to next level

1: // PT
    movl 8(%ebp), %edi // virt_addr low
    shr $12, %edi // Get PT index
    andl $0x1ff, %edi // Mask to 9 bits
    movl (%esi, %edi, 8), %eax // Get PT entry
    testl $1, %eax // Check if present
    jnz 1f // Present

    btl $NX_BIT, (cpu_features - 9b)(%ebx)
    jnc 1f // CPU doesn't support NX bit, skip setting NX bit
    testl $MAP_EXECUTABLE, 24(%ebp)
    jnz 1f // Executable, skip setting NX bit

    movl $0x80000000, 1(%esi, %edi, 8) // Set NX bit in entry
2:
    movl 16(%ebp), %eax // phys_addr
    btl $MAP_WRITABLE, 20(%ebp)
    jnc 3f // Not writable
    orl $0b10, %eax // Set writable bit
3:
    orl $1, %eax // Set present bit
    movl %eax, (%esi, %edi, 8) // Write PT entry
    jmp 2f

1: // Check the present page...
    // 1. Check writable bit
    testl $MAP_WRITABLE, 20(%ebp)
    jnz 3f // Writable
    testl $0b10, %eax
    jnz bad_sections // Page is present but not writable, but kernel requires it to be writable, fail
    jmp 4f
3:
    testl $0b10, %eax
    jz bad_sections // Page is present but not writable, but kernel requires it to be writable, fail

4: // Check executable bit
    btl $NX_BIT, (cpu_features - 9b)(%ebx)
    jnc 2f // CPU doesn't support NX bit, skip checking it

    movl 1(%esi, %edi, 8), %eax

    testl $MAP_EXECUTABLE, 24(%ebp)
    jz 1f

    testl $0x80000000, %eax
    jnz bad_sections // Page is executable but NX bit is set, but kernel requires it
    jmp 2f
1:
    testl $0x80000000, %eax
    jz bad_sections

2:
    movl -12(%ebp), %ebx
    movl -8(%ebp), %edi
    movl -4(%ebp), %esi
    movl %ebp, %esp
    popl %ebp
    ret

bad_sections:
    subl $12, %esp
    leal (bad_sections_msg - 9b)(%ebx), %eax
    pushl %eax
    call multiboot_print_text
    addl $16, %esp

    jmp halt


// =================== .bss ===================
    .bss
    .align 4
cpu_features: .long 0
    .globl multiboot_cr3
multiboot_cr3: .long 0

// =================== .rodata ===================
    .section .rodata
unsupported_cpu_msg:
    .asciz "Unsupported CPU. This kernel requires a 64 bit CPU. Consider using different hardware or a 32 bit version of the kernel."
bad_sections_msg:
    .asciz "Kernel ELF has overlapping sections with incompatible permissions! The kernel can't boot!\r\n"