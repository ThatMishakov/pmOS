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
.equ CPUID_5LVL_PAGING_BIT, 16

.equ NX_BIT, 0
.equ PAGING_5LVL_BIT, 1

.equ MAP_EXECUTABLE, 1
.equ MAP_WRITABLE, 2

.equ pheader_size, 64
.equ PT_LOAD, 1

.equ EFER_MSR, 0xc0000080
.equ EFER_LM_ENABLE_BIT, 8

.equ CR0_PG_BIT, 31
.equ CR4_PAE_BIT, 5
.equ CR4_LA57_BIT, 12

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

    // Check NX
    btl $CPUID_NX_BIT, %edx // Check if NX bit is supported
    jnc 2f // NX bit not supported
    bts $NX_BIT, (cpu_features - 9b)(%edi) // Set NX bit in features
2:

    xorl %eax, %eax
    cpuid

    cmpl $7, %eax
    jb 1f // Leaf 7 not supported, skip checking for 5-level paging

    movl $7, %eax
    xorl %ecx, %ecx
    cpuid

    testl $(1 << CPUID_5LVL_PAGING_BIT), %ecx // Check if 5-level paging is supported
    jz 1f // 5-level paging not supported
    bts $PAGING_5LVL_BIT, (cpu_features - 9b)(%edi) // Set 5-level paging bit in features
1:

    // Alloc cr3
    call multiboot_alloc_page_clear
    movl %eax, multiboot_cr3 - 9b(%edi)

    // Map the kernel
    leal (__ehdr_start - 9b)(%edi), %ebx
    // %esi = phnum
    movzwl 56(%ebx), %esi
    // %esi = phnum * 64
    shl $6, %esi
    // %ebx = current address of the first program header
    addl 32(%ebx), %ebx
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
    pushl 4(%ebx) // flags
    andl $3, (%esp) // Keep writable and executable bits

    movl 24(%ebx), %eax // paddr
    addl (multiboot_kernel_phys_offset - 9b)(%edi), %eax // Offset addr by what it was moved by the bootloader
    pushl %eax // phys addr

    pushl 40(%ebx) // memsz
    pushl 20(%ebx) // p_vaddr high
    pushl 16(%ebx) // p_vaddr low
    call bootstrap_map
    addl $32, %esp

3:
    addl $pheader_size, %ebx
    jmp 1b
2:

    call identity_map_memory

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

    .type identity_map_memory, @function
identity_map_memory:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    call 1f
1:
    popl %eax

    movl (multiboot_cr3 - 1b)(%eax), %esi // %esi = current page table base

    // PLM5?
    btl $PAGING_5LVL_BIT, (cpu_features - 1b)(%eax)
    jnc 1f

    movl (%esi), %eax // Get PML5 entry
    testl $1, %eax // Check if present
    jnz 2f // Present
    call multiboot_alloc_page_clear
    movl %eax, (%esi) // Write PML5 entry
    orl $0b11, (%esi) // Set present and writable bits
    movl %eax, %esi // Move to next level
    jmp 1f
2:
    movl (%esi), %esi
    andl $0xfffff000, %esi // Get address of next level page table


1: // PML4
    movl (%esi), %eax // Get PML4 entry
    testl $1, %eax // Check if present
    jnz 2f // Present
    call multiboot_alloc_page_clear
    movl %eax, (%esi) // Write the entry 
    orl $0b11, (%esi) // Set present and writable bits
    movl %eax, %esi // Move to next level
    jmp 1f
2:
    movl (%esi), %esi
    andl $0xfffff000, %esi // Get address of next level page table

1: // PDPT
    movl $0, %edi
1:
    call multiboot_alloc_page_clear
    movl %eax, (%esi, %edi, 8) // Write the entry
    orl $0b11, (%esi, %edi, 8)

    movl %edi, %ecx
    shl $30, %ecx

    pushl %ecx
    pushl %eax
    call populate_pdpt
    addl $8, %esp

    incl %edi
    cmpl $4, %edi
    jb 1b

    popl %edi
    popl %esi
    movl %ebp, %esp
    popl %ebp
    ret

    // void populate_pdpt(uint32_t pdpt_phys_addr, uint32_t virt_base)
    .type populate_pdpt, @function
populate_pdpt:
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

    .globl multiboot_enter_kernel
    .type multiboot_enter_kernel, @function
multiboot_enter_kernel:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    subl $4, %esp

    call 9f
9:
    popl %ebx

    subl $16, %esp
    // GDTR
    movl $31, 2(%esp)
    leal (multiboot_gdt - 9b)(%ebx), %edx
    movl %edx, 4(%esp)

    lgdt 2(%esp)

    // Enable PAE and maybe 5-level paging
    movl %cr4, %eax
    orl $(1 << CR4_PAE_BIT), %eax
    testl $(1 << PAGING_5LVL_BIT), (cpu_features - 9b)(%ebx)
    jz 1f
    orl $(1 << CR4_LA57_BIT), %eax
1:
    movl %eax, %cr4

    // Enable compat mode
    movl $EFER_MSR, %ecx
    rdmsr
    orl $(1 << EFER_LM_ENABLE_BIT), %eax
    wrmsr

    // Enable paging...
    movl (multiboot_cr3 - 9b)(%ebx), %eax
    movl %eax, %cr3

    movl %cr0, %eax
    orl $(1 << CR0_PG_BIT), %eax
    movl %eax, %cr0

    // Jump to 64 bit code
    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss

    // Far return to 64 bit code
    leal (1f - 9b)(%ebx), %eax
    pushl $0x08
    pushl %eax
    lretl
1:

    .code64
    movabsq $multiboot_entry, %rax
    jmp *%rax


    .code32
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

    call 9f
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
    movl %eax, %ecx
    orl $0b11, %ecx // Set present and writable bits
    movl %ecx, (%esi, %edi, 8) // Write PML5 entry
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
    jmp 1f
2:
    call multiboot_alloc_page_clear
    movl %eax, %ecx
    orl $0b11, %ecx // Set present and writable bits
    movl %ecx, (%esi, %edi, 8) // Write PML5 entry
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
    jmp 1f
2:
    call multiboot_alloc_page_clear
    movl %eax, %ecx
    orl $0b11, %ecx // Set present and writable bits
    movl %ecx, (%esi, %edi, 8) // Write PDPT
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
    jmp 1f
2:
    call multiboot_alloc_page_clear
    movl %eax, %ecx
    orl $0b11, %ecx // Set present and writable bits
    movl %ecx, (%esi, %edi, 8) // Write PD entry
    movl %eax, %esi // Move to next level

1: // PT
    movl 8(%ebp), %edi // virt_addr low
    shr $12, %edi // Get PT index
    andl $0x1ff, %edi // Mask to 9 bits
    movl (%esi, %edi, 8), %eax // Get PT entry
    testl $1, %eax // Check if present
    jnz 1f // Present

    btl $NX_BIT, (cpu_features - 9b)(%ebx)
    jnc 2f // CPU doesn't support NX bit, skip setting NX bit
    testl $MAP_EXECUTABLE, 20(%ebp)
    jnz 2f // Executable, skip setting NX bit

    movl $0x80000000, 4(%esi, %edi, 8) // Set NX bit in entry
2:
    movl 16(%ebp), %eax // phys_addr
    testl $MAP_WRITABLE, 20(%ebp)
    jz 3f // Not writable
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

    movl 4(%esi, %edi, 8), %eax

    testl $MAP_EXECUTABLE, 20(%ebp)
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

// =================== .data ===================
    .data
multiboot_gdt:
    .quad 0
    .quad 0x00af9b000000ffff // 64 bit code
    .quad 0x00cf92000000ffff // 64/32 bit data
    .quad 0x00cf9a000000ffff // 32 bit code