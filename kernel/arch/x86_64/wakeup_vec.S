    .p2align 4, 0xCC
    .data
    .globl init_vec_begin
    .type init_vec_begin, @function
init_vec_begin:
    .code16

    cli
    xchgw %bx, %bx
    movw %cs, %bx

    lgdt %cs:(acpi_trampoline_gdtr - init_vec_begin)

    movl $0x01, %edx # Protected mode
    movl %edx, %cr0

    ljmpl *%cs:(init_vec_jump_pmode - init_vec_begin)
init_vec_jump_instr:
    .code32

    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movzx %bx, %ebx
    shll $4, %ebx
    
    movl (acpi_trampoline_startup_cr3 - init_vec_begin)(%ebx), %eax
    movl %eax, %cr3 # cr3 (Page table)

    movl %cr4, %eax # PAE 
    orl $(1 << 5), %eax
    movl %eax, %cr4

    movl $(1 << 8), %eax # LME BIT
    xorl %edx, %edx
    movl $0xC0000080, %ecx # IA32_EFER
    wrmsr 

    # Enable paging
    movl $0x80000001, %eax
    movl %eax, %cr0

    leal (scratch_stack - init_vec_begin)(%ebx), %esp
    leal (1f - init_vec_begin)(%ebx), %eax
    pushl $0x08
    pushl %eax
    lretl
1:
    .code64
    jmp *init_vec_kernel_entry(%rip)


    .p2align 4, 0xCC
    .code16
    .globl acpi_trampoline_begin
acpi_trampoline_begin:
    .globl acpi_cpu_startup
acpi_cpu_startup:

    movw %cs, %bx

    lgdt %cs:(acpi_trampoline_gdtr - acpi_cpu_startup)

    movl $0x01, %edx # Protected mode
    movl %edx, %cr0

    ljmpl *%cs:(protected_acpi_vec_far_jump - acpi_cpu_startup)
acpi_protected_far_jump_instr:
    .code32

    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movzx %bx, %ebx
    shll $4, %ebx
    
    movl (acpi_trampoline_startup_cr3 - acpi_cpu_startup)(%ebx), %eax
    movl %eax, %cr3 # cr3 (Page table)

    movl %cr4, %eax # PAE 
    orl $(1 << 5), %eax
    movl %eax, %cr4

    movl $(1 << 8), %eax # LME BIT
    xorl %edx, %edx
    movl $0xC0000080, %ecx # IA32_EFER
    wrmsr 

    # Enable paging
    movl $0x80000001, %eax
    movl %eax, %cr0

    leal (acpi_trampoline_kernel_entry - acpi_cpu_startup)(%ebx), %esp
    leal (_64bitst - acpi_cpu_startup)(%ebx), %eax
    pushl $0x08
    pushl %eax
    lretl
_64bitst:
    .code64
    jmp *acpi_trampoline_kernel_entry(%rip)

    .p2align 4, 0xCC

    .globl protected_acpi_vec_far_jump
protected_acpi_vec_far_jump:
    .long (acpi_protected_far_jump_instr - init_vec_begin) # offset
    .word 0x18 # 32 bit cs

    .space 2

    .globl init_vec_jump_pmode
init_vec_jump_pmode:
    .long (init_vec_jump_instr - init_vec_begin)
    .word 0x18
    
acpi_trampoline_gdtr:
acpi_trampoline_gdtr_limit:
    .word 31 # 3 entries - 1
    .globl acpi_tampoline_gdtr_addr
acpi_tampoline_gdtr_addr:
    .long (acpi_trampoline_gdt - init_vec_begin)

acpi_trampoline_gdt:
    .quad 0
    .quad 0x00af9b000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00cf9a000000ffff
    .globl acpi_trampoline_startup_cr3
acpi_trampoline_startup_cr3:
    .space 4

smp_trampoline_scratch_stack:
    .space 8
scratch_stack:
    .space 8
acpi_trampoline_kernel_entry:
    .quad _acpi_wakeup_entry

init_vec_kernel_entry:
    .quad _smp_trampoline_entry

    .globl acpi_trampoline_startup_end
acpi_trampoline_startup_end:
