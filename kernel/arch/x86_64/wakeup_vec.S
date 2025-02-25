    .data
init_vec_stuff:
    .code16

    .globl acpi_trampoline_begin
acpi_trampoline_begin:
    .globl acpi_cpu_startup
acpi_cpu_startup:
    cli

    movw %cs, %bx

    xorl %ecx, %ecx
    movw %bx, %cx

    movl $0x01, %edx # Protected mode
    movl %edx, %cr0

    jmp 1f
1:
    .code32

    shll $4, %ecx
    leal (acpi_trampoline_gdt - acpi_cpu_startup)(%ecx), %edx
    movl %edx, %cs:(acpi_tampoline_gdtr_addr - acpi_cpu_startup)

    lgdt (acpi_trampoline_gdtr - acpi_cpu_startup)(%ecx)
    
    movl (acpi_trampoline_startup_cr3 - acpi_cpu_startup)(%ecx), %eax
    movl %eax, %cr3 # cr3 (Page table)

    movl %cr4, %eax # PAE 
    orl $(1 << 5), %eax
    movl %eax, %cr4

    movl $(1 << 10), %eax # LME BIT
    xorl %edx, %edx
    movl $0xC0000080, %ecx # IA32_EFER
    wrmsr 

    # Enable paging
    movl $80000001, %eax
    movl %eax, %cr0

    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    shll $16, %ebx
    leal (acpi_trampoline_kernel_entry - acpi_cpu_startup)(, %ebx, 1), %esp
    leal (_64bitst - acpi_cpu_startup)(, %ebx, 1), %eax
    pushl %eax
    lretl $0x08
_64bitst:
    .code64
    jmp _64bitst
    jmp *acpi_trampoline_kernel_entry(%rip)

    .p2align 4, 0xCC

acpi_trampoline_gdtr:
    .space 2 # alignment 
acpi_trampoline_gdtr_limit:
    .word 23 # 3 entries - 1
acpi_tampoline_gdtr_addr:
    .long 0

acpi_trampoline_gdt:
    .quad 0
    .quad 0x00af9b000000ffff
    .quad 0x00af93000000ffff
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
