.extern multiboot_print_text
.extern multiboot_alloc_page
.extern multiboot_alloc_page_clear
.extern __ehdr_start
.extern __ehdr_end

.extern multiboot_kernel_phys_offset
.extern pae_populate_pdpt

.equ NX_BIT, 0
.equ PAGING_5LVL_BIT, 1
.equ PAE_BIT, 2
.equ PSE_BIT, 3

.equ CPUID_EXTENDED_MAX, 0x80000000
.equ CPUID_EXTENDED_FEATURES, 0x80000001

.equ CPUID_PSE_BIT, 3
.equ CPUID_PAE_BIT, 6

.equ pheader_size, 32
.equ PT_LOAD, 1

.equ CR0_PG_BIT, 31
.equ CR4_PSE_BIT, 4
.equ CR4_PAE_BIT, 5

.equ EFER_MSR, 0xc0000080
.equ EFER_NX_ENABLE_BIT, 11

.equ MAP_EXECUTABLE, 1
.equ MAP_WRITABLE, 2

.equ CPUID_NX_BIT, 20

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
    jz 1f

    // Check max leaf
    xorl %eax, %eax
    cpuid
    movl %eax, %esi
    cmpl $1, %eax
    jb 2f

    // Check PSE and PAE
    movl $0x01, %eax
    cpuid
    btl $CPUID_PSE_BIT, %edx
    jnc 1f // No PSE support, can't use large pages
    orl $1 << PSE_BIT, (multiboot_cpu_features - 9b)(%edi) // Set PSE bit in features
1:
    btl $CPUID_PAE_BIT, %edx
    jnc 2f // PAE not supported
    orl $1 << PAE_BIT, (multiboot_cpu_features - 9b)(%edi) // Set PAE bit in features

    // Check NX if there's PAE
    movl $CPUID_EXTENDED_FEATURES, %eax
    cpuid
    cmpl $CPUID_EXTENDED_FEATURES, %eax
    jb 2f // Extended features not supported, skip checking for NX

    movl $CPUID_EXTENDED_FEATURES, %eax
    cpuid

    // Check NX
    btl $CPUID_NX_BIT, %edx // Check if NX bit is supported
    jnc 2f // NX bit not supported
    bts $NX_BIT, (multiboot_cpu_features - 9b)(%edi) // Set NX bit in features
2:

    // Alloc cr3
    call multiboot_alloc_page_clear
    movl %eax, multiboot_cr3 - 9b(%edi)

    // Map the kernel
    leal (__ehdr_start - 9b)(%edi), %ebx
    // %esi = phnum
    movzwl 44(%ebx), %esi
    // %esi = phnum * 64
    imull $pheader_size, %esi
    // %ebx = current address of the first program header
    addl 28(%ebx), %ebx
    // %esi = address of the end of the program headers
    addl %ebx, %esi
1:
    cmpl %esi, %ebx
    jae 2f

    // Check if loadable
    cmpl $PT_LOAD, (%ebx) // Check if it's a loadable segment
    jne 3f // Not loadable, skip

    pushl 24(%ebx) // flags
    andl $3, (%esp) // Keep writable and executable bits

    movl 12(%ebx), %eax // paddr
    addl (multiboot_kernel_phys_offset - 9b)(%edi), %eax // Offset addr by what it was moved by the bootloader
    pushl %eax // phys addr

    pushl 20(%ebx) // memsz
    pushl 8(%ebx) // p_vaddr
    call bootstrap_map
    addl $16, %esp

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


    .type identity_map_memory, @function
identity_map_memory:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    pushl %ebx
    pushl %esi
    pushl %edi

    call 9f
9:
    popl %ebx

    movl (multiboot_cr3 - 9b)(%ebx), %esi

    testl $1 << PAE_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jz 1f

    movl $0, %edi
2:
    call multiboot_alloc_page_clear
    movl %eax, (%esi, %edi, 8)
    orl $0x1, (%esi, %edi, 8) // Present + RW

    movl %edi, %ecx
    shl $30, %ecx
    pushl %ecx
    pushl %eax
    call pae_populate_pdpt
    addl $8, %esp

    incl %edi
    cmpl $2, %edi
    jle 2b
    jmp 3f

1: // No PAE

    // Fill the first page directory without PSE
    call multiboot_alloc_page_clear
    movl %eax, (%esi)
    orl $0x3, (%esi) // Present + Write

    subl $8, %esp
    pushl $0 // Virt base
    pushl %eax // phys addr
    call fill_pt_identity
    addl $16, %esp

    movl $1, %edi
    testl $1 << PSE_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jz 2f
1:
    movl %edi, %eax
    shl $22, %eax // Each PDE covers 4MB
    orl $0x83, %eax // PS + Write + Present
    movl %eax, (%esi, %edi, 4)

    incl %edi
    cmpl $768, %edi // 768 * 4MB = 3GB
    jb 1b
    jmp 1f
2: // No PSE loop
    call multiboot_alloc_page_clear
    movl %eax, (%esi, %edi, 4)
    orl $0x3, (%esi, %edi, 4) // Present + Write

    movl %esi, %ecx
    shl $22, %ecx // Each PDE covers 4MB
    subl $8, %esp
    pushl %ecx // phys addr
    pushl %eax // virt addr
    call fill_pt_identity
    addl $16, %esp

    incl %edi
    cmpl $768, %edi // 768 * 4MB = 3GB
    jb 2b

3: // Done
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

    // void fill_pt_identity(uint32_t pt_phys_addr, uint32_t virt_base)
    .type fill_pt_identity, @function
fill_pt_identity:
    pushl %ebp
    movl %esp, %ebp

    movl 8(%ebp), %eax // pt_phys_addr
    movl 12(%ebp), %ecx // virt_base
    orl $3, %ecx // Present + RW

    movl $0, %edx
1:
    movl %ecx, (%eax, %edx, 4)
    addl $0x1000, %ecx
    incl %edx
    cmpl $1024, %edx
    jb 1b
    movl %ebp, %esp
    popl %ebp
    ret


    // void bootstrap_map(uint32_t virt_addr, uint32_t size, uint32_t phys_addr, uint32_t flags)
    .type bootstrap_map, @function
bootstrap_map:
    pushl %ebp
    movl %esp, %ebp
    pushl %esi
    pushl %edi

    // Round size up to page boundary
    movl 8(%ebp), %eax // virt_addr
    andl $0xfff, %eax
    subl %eax, 8(%ebp) // virt_addr
    subl %eax, 16(%ebp) // phys_addr
    addl %eax, 12(%ebp) // Round size up to page boundary
    addl $0xfff, 12(%ebp)
    andl $0xfffff000, 12(%ebp)

    movl $0, %edi
1:
    cmpl 12(%ebp), %edi
    jae 1f

    subl $4, %esp
    pushl 20(%ebp) // flags
    pushl 16(%ebp) // phys_addr
    addl %edi, (%esp) // phys_addr + offset
    pushl 8(%ebp) // virt_addr
    addl %edi, (%esp) // virt_addr + offset
    call map_page
    addl $16, %esp

    addl $0x1000, %edi
    jmp 1b

1: // End
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

    // Enable PSE and PAE if supported
    movl %cr4, %eax
    testl $1 << PSE_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jz 1f
    orl $1 << CR4_PSE_BIT, %eax
1:
    testl $1 << PAE_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jz 1f
    orl $1 << CR4_PAE_BIT, %eax
1:
    movl %eax, %cr4

    // Enable NX
    testl $(1 << NX_BIT), (multiboot_cpu_features - 9b)(%ebx)
    jz 1f

    movl $EFER_MSR, %ecx
    rdmsr
    orl $(1 << EFER_NX_ENABLE_BIT), %eax
    wrmsr
1:

    movl (multiboot_cr3 - 9b)(%ebx), %eax
    movl %eax, %cr3

    movl %cr0, %eax
    orl $1 << CR0_PG_BIT, %eax
    movl %eax, %cr0

    movl $stack_end, %esp
    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp
    pushl %esi

    movl $1f, %eax
    jmp *%eax
1:
    call multiboot_main

    jmp return_from_kernel


    // void map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flags)
    .type map_page, @function
map_page:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    subl $12, %esp

    call 9f
9:
    popl %ebx

    movl (multiboot_cr3 - 9b)(%ebx), %esi

    testl $1 << PAE_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jz 1f

    movl 8(%ebp), %edi // virt_addr
    shr $30, %edi // Get PDPT index
    movl (%esi, %edi, 8), %eax // Get PDPT entry
    testl $1, %eax
    jz 2f // Not present

    movl %eax, %esi
    andl $0xfffff000, %esi
    jmp 3f
2:
    call multiboot_alloc_page_clear

    movl %eax, %edx
    orl $0x1, %edx // Present
    movl %edx, (%esi, %edi, 8) // Update PDPT entry
    movl %eax, %esi


3: // PD

    movl 8(%ebp), %edi // virt_addr
    shr $21, %edi // Get PD index
    andl $0x1ff, %edi
    movl (%esi, %edi, 8), %eax // Get PD entry
    testl $1, %eax
    jz 2f // Not present

    movl %eax, %esi
    andl $0xfffff000, %esi
    jmp 3f
2:
    call multiboot_alloc_page_clear

    movl %eax, %edx
    orl $0x3, %edx // Present + Write
    movl %edx, (%esi, %edi, 8) // Update PDPT entry
    movl %eax, %esi

3: // PT
    movl 8(%ebp), %edi // virt_addr
    shr $12, %edi // Get PT index
    andl $0x1ff, %edi
    movl (%esi, %edi, 8), %eax // Get PT entry
    testl $1, %eax
    jnz 2f // Present...

    movl 12(%ebp), %eax // phys_addr
    testl $1 << NX_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jz 3f // NX not supported, skip setting the bit
    testl $MAP_EXECUTABLE, 16(%ebp)
    jnz 3f // Executable, skip setting NX bit
    movl $0x80000000, 4(%esi, %edi, 8)
3:
    orl $0x1, %eax // Present
    testl $MAP_WRITABLE, 16(%ebp)
    jz 3f
    orl $0x2, %eax // Writable
3:
    movl %eax, (%esi, %edi, 8) // Update PT entry
    jmp 4f

2: // Check the present page...
    // 1. Check writable bit
    testl $MAP_WRITABLE, 20(%ebp)
    jnz 3f // Writable
    testl $0b10, %eax
    jnz bad_sections // Page is present but not writable, but kernel requires it to be writable, fail
    jmp 2f
3:
    testl $0b10, %eax
    jz bad_sections // Page is present but not writable, but kernel requires it to be writable, fail
2:
    // Check executable bit
    btl $NX_BIT, (multiboot_cpu_features - 9b)(%ebx)
    jnc 4f // CPU doesn't support NX bit, skip checking it

    movl 4(%esi, %edi, 8), %eax // Check NX bit
    movl %eax, %edx
    xorl $0x01, %edx // Invert the executable bit
    andl $1, %edx
    shl $31, %edx
    xorl %edx, %eax
    jnz bad_sections // One bit is set, the other one is clear

    jmp 4f
1: // No PAE

    movl 8(%ebp), %edi // virt_addr
    shr $22, %edi // Get PDE index
    movl (%esi, %edi, 4), %eax // Get PDE entry
    testl $1, %eax
    jz 2f // Not present

    movl %eax, %esi
    andl $0xfffff000, %esi
    jmp 3f
2:
    call multiboot_alloc_page_clear

    movl %eax, %edx
    orl $0x3, %edx // Present + Write
    movl %edx, (%esi, %edi, 4) // Update PDE entry
    movl %eax, %esi

3:
    movl 8(%ebp), %edi // virt_addr
    shr $12, %edi // Get PT index
    andl $0x3ff, %edi
    movl (%esi, %edi, 4), %eax // Get PT entry
    testl $1, %eax
    jnz 2f // Present...

    movl 12(%ebp), %eax // phys_addr
    orl $0x1, %eax // Present
    testl $MAP_WRITABLE, 16(%ebp)
    jz 3f
    orl $0x2, %eax // Writable
3:
    movl %eax, (%esi, %edi, 4) // Update PT entry
    jmp 4f

2: // Check the present page...
    movl 16(%ebp), %ecx // flags
    andl $2, %ecx // Only keep the writable bit
    orl 12(%ebp), %ecx // Writable + phys addr
    xorl %eax, %ecx
    andl $0xfffff002, %ecx // Check phys addr and writable bit
    jnz bad_sections // Page is present but doesn't have the required writable bit or phys addr

4: // end
    addl $12, %esp
    popl %edi
    popl %esi
    popl %ebx
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


.type halt, @function
halt:
    cli
    hlt
    jmp halt

    // =================== .bss ===================
    .bss
    .align 4
    .globl multiboot_cpu_features
multiboot_cpu_features: .long 0
    .globl multiboot_cr3
multiboot_cr3: .long 0

    // =================== .rodata ===================
    .rodata
bad_sections_msg:
    .asciz "Kernel ELF has overlapping sections with incompatible permissions! The kernel can't boot!\r\n"