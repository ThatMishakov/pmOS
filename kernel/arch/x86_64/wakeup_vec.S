    .data
init_vec_stuff:
    .code16

    .globl acpi_trampoline_begin
acpi_trampoline_begin:
    .globl acpi_cpu_startup
acpi_cpu_startup:
    cli

    xchgw %bx, %bx

    movw %cs, %bx
    movw %bx, %cx
    shlw $4, %cx
    addw $(acpi_trampoline_gdt - acpi_cpu_startup), %cx
    movw %cx, %cs:(acpi_tampoline_gdtr_addr - acpi_cpu_startup)
    movw %bx, %cx
    shrw $12, %cx
    movw %cx, %cs:(acpi_tampoline_gdtr_addr - acpi_cpu_startup + 2)
    lgdt %cs:(acpi_trampoline_gdtr - acpi_cpu_startup)

    movl $0x01, %edx # Protected mode
    movl %edx, %cr0

    movw %bx, %cx
    shlw $4, %cx
    addw $(1f - acpi_cpu_startup), %cx
    movw %cx, %cs:(protected_far_jump - acpi_cpu_startup)
    movw %bx, %cx
    shrw $12, %cx
    movw %cx, %cs:(protected_far_jump - acpi_cpu_startup + 2)

    ljmpl *%cs:(protected_far_jump - acpi_cpu_startup)
1:
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
    jmp _64bitst
    jmp *acpi_trampoline_kernel_entry(%rip)

    .p2align 4, 0xCC

protected_far_jump:
    .long 0 # offset
    .word 0x18 # 32 bit cs
    
acpi_trampoline_gdtr:
acpi_trampoline_gdtr_limit:
    .word 31 # 3 entries - 1
acpi_tampoline_gdtr_addr:
    .long 0

acpi_trampoline_gdt:
    .quad 0
    .quad 0x00af9b000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00cf9a000000ffff
    .globl acpi_trampoline_startup_cr3
acpi_trampoline_startup_cr3:
    .space 4


    .globl acpi_trampoline_kernel_entry
acpi_trampoline_kernel_entry:
    .space 8
scratch_stack:
    .space 8

    .globl acpi_trampoline_startup_end
acpi_trampoline_startup_end:
