    .bss
bootstrap_stack_bottom:
    .skip 16384
bootstrap_stack_top:
    .skip 4096
smp_temp_stack_top:
spinlock:
    .long 0

    .text
    .globl _kmain_entry
    .type _kmain_entry, @function
_kmain_entry:
    movl 4(%esp), %eax
    movl 8(%esp), %edx

    movl $bootstrap_stack_top, %esp

    # Align the stack to 16 bytes
    subl $8, %esp

    pushl %edx
    pushl %eax

    call kmain

    jmp return_from_kernel

    .globl _kernel_smp_entry
    .type _kernel_smp_entry, @function
_kernel_smp_entry:
    leal smp_temp_stack_top, %esp
    xorl %ebx, %ebx

    lock btsl $0, spinlock
    jnc acquired
1:
    pause
    testl $1, spinlock
    jnz 1b
    lock btsl $0, spinlock
    jc 1b
acquired:

    call find_cpu_info

    movl $0, spinlock # Unlock

    movl 4(%eax), %esp
    subl $12, %esp
    pushl %eax
    call smp_main


    jmp return_from_kernel


    .globl _acpi_wakeup_entry
    .type _acpi_wakeup_entry, @function
_acpi_wakeup_entry:

    movl $bootstrap_stack_top, %esp
    xorl %ebp, %ebp

    call acpi_main

    jmp return_from_kernel