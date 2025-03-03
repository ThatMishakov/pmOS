    .p2align 4, 0xCC
    .data
    .globl init_vec_begin
    .type init_vec_begin, @function
init_vec_begin:
    .code16

    movw %cs, %bx
    
    lgdt %cs:(smp_trampoline_gdtr - init_vec_begin)

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

    movl (smp_trampoline_cr3 - init_vec_begin)(%ebx), %eax
    movl %eax, %cr3 # cr3 (Page table)

    testl $0x01, (smp_trampoline_trampoilne_flags - init_vec_begin)(%ebx)
    jz init_no_pae
    
    movl %cr4, %eax # PAE 
    orl $(1 << 5), %eax
    movl %eax, %cr4

    testl $0x02, (smp_trampoline_trampoilne_flags - init_vec_begin)(%ebx)
    jz init_no_pae # No NX

    movl $(1 << 11), %eax # Execute Disable Bit Enable
    xorl %edx, %edx
    movl $0xC0000080, %ecx # IA32_EFER
    wrmsr 

init_no_pae:

    # Enable paging
    movl $0x80000001, %eax
    movl %eax, %cr0

    jmpl *(init_vec_kernel_entry - init_vec_begin)(,%ebx)



    .p2align 4, 0xCC
    .code16
    .globl acpi_trampoline
    .type acpi_trampoline, @function
acpi_trampoline:

    movw %cs, %bx
    
    lgdt %cs:(smp_trampoline_gdtr - acpi_trampoline)

    movl $0x01, %edx # Protected mode
    movl %edx, %cr0

    ljmpl *%cs:(acpi_vec_jump_pmode - acpi_trampoline)
acpi_trampoline_jump_pmode:
    .code32

    movl $0x10, %eax
    movw %ax, %ds
    movw %ax, %ss
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    movzx %bx, %ebx
    shll $4, %ebx

    movl (smp_trampoline_cr3 - acpi_trampoline)(%ebx), %eax
    movl %eax, %cr3 # cr3 (Page table)

    testl $0x01, (smp_trampoline_trampoilne_flags - acpi_trampoline)(%ebx)
    jz 1f # No PAE
    
    movl %cr4, %eax # PAE 
    orl $(1 << 5), %eax
    movl %eax, %cr4

    testl $0x02, (smp_trampoline_trampoilne_flags - acpi_trampoline)(%ebx)
    jz 1f # No NX

    movl $(1 << 11), %eax # Execute Disable Bit Enable
    xorl %edx, %edx
    movl $0xC0000080, %ecx # IA32_EFER
    wrmsr 

1:

    # Enable paging
    movl $0x80000001, %eax
    movl %eax, %cr0

    jmpl *(acpu_trampoline_kernel_entry - acpi_trampoline)(%ebx)


    .p2align 2, 0xCC
    .globl init_vec_jump_pmode
init_vec_jump_pmode:
    .long (init_vec_jump_instr - init_vec_begin)
    .word 0x08

smp_trampoline_gdtr:
    .word (0x17)
    .globl smp_trampoline_gdtr_addr
smp_trampoline_gdtr_addr:
    .long (acpi_trampoline_gdt - init_vec_begin)

    .globl acpi_vec_jump_pmode
acpi_vec_jump_pmode:
    .long (acpi_trampoline_jump_pmode - init_vec_begin)
    .word 0x08

    .space 2

    .globl smp_trampoline_cr3
smp_trampoline_cr3:
    .long 0

    .globl smp_trampoline_trampoilne_flags
smp_trampoline_trampoilne_flags:
    .long 0

init_vec_kernel_entry:
    .long _kernel_smp_entry

acpu_trampoline_kernel_entry:
    .long _acpi_wakeup_entry

acpi_trampoline_gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff

    .globl init_vec_end
init_vec_end: