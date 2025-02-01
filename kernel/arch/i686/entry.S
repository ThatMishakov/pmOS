    .text
    .type return_from_kernel, @function
    .globl return_from_kernel
return_from_kernel:
    movl %gs:16, %eax
    movl 52(%eax), %edx # Entry type
    jmp *return_table(,%edx,4)

    .macro return_from_kernel_macro
    movl %gs:16, %eax
    movl 52(%eax), %edx # Entry type
    jmp *return_table(,%edx,4)
    .endm

    .type return_from_kernel, @function
    .globl ret_from_interrupt
ret_from_interrupt:
    # Expecting task pointer to be in %eax...

    # Push interrupt stack frame...
    pushl $0x23    # ss
    pushl 28(%eax) # esp
    pushl 36(%eax) # eflags
    pushl 40(%eax) # cs
    pushl 32(%eax) # eip

    # Switch to user gs
    movw $0x2B, %cx
    movw %cx, %gs

    # Restore registers
    movl 4(%eax), %ebx
    movl 8(%eax), %ecx
    movl 12(%eax), %edx
    movl 16(%eax), %esi
    movl 20(%eax), %edi
    movl 24(%eax), %ebp
    movl (%eax), %eax

    # Return
    iretl

    .type return_from_kernel_thread, @function
    .globl return_from_kernel_thread
return_from_kernel_thread:
    # Switch to kernel thread stack
    movl 28(%eax), %esp

    # Push interrupt frame onto the thread's stack
    pushl 36(%eax)
    pushl 40(%eax)
    pushl 32(%eax)

    # Restore registers
    movl  4(%eax), %ebx
    movl  8(%eax), %ecx
    movl 12(%eax), %esi
    movl 16(%eax), %edi
    movl 20(%eax), %ebp
    movl  0(%eax), %eax

    # Casually (being casual is important) return to the kernel thread...
    iretl

    .globl syscall_isr
    .type syscall_isr, @function
syscall_isr:
    pushl %eax
    movw $0x38, %ax
    movw %ax, %gs

    movl %gs:16, %eax
    popl 0(%eax) # Save eax
    movl %ebx, 4(%eax)
    movl %ecx, 8(%eax)
    movl %edx, 12(%eax)
    movl %esi, 16(%eax)
    movl %edi, 20(%eax)
    movl %ebp, 24(%eax)

    popl 32(%eax) # eip
    popl 40(%eax) # cs
    popl 36(%eax) # eflags
    popl 28(%eax) # esp
    # popl 44(%eax) # ss
    addl $4, %esp

    xorl %ecx, %ecx
    movl %ecx, 52(%eax) # Entry type

    call syscall_handler

    jmp return_from_kernel

    .globl ret_repeat_syscall
    .type ret_repeat_syscall, @function
ret_repeat_syscall:
    movl 56(%eax), %ecx
    movl %ecx, 52(%eax) # Restore entry type
    xorl %ecx, %ecx
    movl %ecx, 56(%eax)

    call syscall_handler

    jmp return_from_kernel


    .type syscall_entry, @function
    .globl syscall_entry
syscall_entry:
    # Sorry not sorry, but trash %edi
    movw $0x38, %di
    movw %di, %gs

    # Now we're talking :)
    movl %gs:16, %edi
    movl %esp, 28(%edi) # Save esp
    movl %ecx, 32(%edi) # Save eip
    movl %eax, 0(%edi) # Save eax
    movl %ebx, 4(%edi)
    movl %esi, 8(%edi) # Pretend %esi is %ecx (2nd syscall argument)
    movl %edx, 12(%edi)
    movl %esi, 16(%edi)
    movl %ebp, 24(%edi)

    xorl %eax, %eax
    movl %eax, 20(%edi) # Zero out %edi (to help with kernel pointers leaking to userland)

    # Entry type
    movl $1, %eax
    movl %eax, 52(%edi) # Entry type

    movl %gs:4, %esp # Load kernel stack...

    # store flags
    pushfl
    popl 36(%edi)

    # Align the stack to 16 bytes
    pushl $0
    pushl $0
    movl %esp, %ebp
    subl $8, %esp

    call syscall_handler

    jmp return_from_kernel

    .type apic_spurious_isr, @function
    .globl apic_spurious_isr
apic_spurious_isr:
    iret


    .type apic_timer_isr, @function
    .globl apic_timer_isr
apic_timer_isr:
    pushl $0 # dummy error code
    call save_registers

    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp

    call timer_interrupt

    return_from_kernel_macro

    .type apic_dummy_isr, @function
    .globl apic_dummy_isr
apic_dummy_isr:
    pushl $0
    call save_registers

    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp

    call apic_dummy_int_routine

    return_from_kernel_macro

    .type ipi_reschedule_isr, @function
    .globl ipi_reschedule_isr
ipi_reschedule_isr:
    pushl $0
    call save_registers

    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp

    call reschedule_isr

    return_from_kernel_macro

    .type ipi_invalidate_tlb_isr, @function
    .globl ipi_invalidate_tlb_isr
ipi_invalidate_tlb_isr:
    pushl $0
    call save_registers

    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp

    call ipi_invalidate_tlb_routine

    return_from_kernel_macro

    .type apic_lvt0_isr, @function
    .globl apic_lvt0_isr
apic_lvt0_isr:
    pushl $0
    call save_registers

    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp

    call lvt0_int_routine

    return_from_kernel_macro

    .type apic_lvt1_isr, @function
    .globl apic_lvt1_isr
apic_lvt1_isr:
    pushl $0
    call save_registers

    subl $4, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp

    call lvt1_int_routine

    return_from_kernel_macro


    .type ret_from_syscall, @function
    .globl ret_from_syscall
ret_from_syscall: # Best function naming conventions in the world...
    # Load user %gs
    movw $0x28, %cx
    movw %cx, %gs

    # Restore registers

    pushl 36(%eax) # eflags
    popfl
    movl 4(%eax), %ebx
    movl 32(%eax), %ecx # eip
    movl 12(%eax), %edx
    movl 16(%eax), %esi
    movl 20(%eax), %edi
    movl 24(%eax), %ebp
    movl 28(%eax), %esp
    movl 0(%eax), %eax

    # Return
    sysretl

    .type ret_from_sysenter, @function
    .globl ret_from_sysenter
ret_from_sysenter:
    # Load user %gs
    movw $0x28, %cx
    movw %cx, %gs

    # Restore registers
    pushl 36(%eax) # eflags
    popfl

    movl 4(%eax), %ebx
    movl 32(%eax), %ecx # eip
    movl 28(%eax), %edx # esp
    movl 12(%eax), %esi
    movl 16(%eax), %edi
    movl 24(%eax), %ebp
    movl 0(%eax), %eax

    # Return
    sysretl

    .type double_fault_isr, @function
    .globl double_fault_isr
double_fault_isr:
    # Double fault is entered with Task Gate...

    pushl $0
    pushl $0
    movl %esp, %ebp
    subl $8, %esp

    call double_fault_handler

    addl $16, %esp

    iretl

    .type save_registers, @function
save_registers:
    pushl %eax
    movw $0x38, %ax
    movl %ax, %gs

    movw $0x1b, %ax
    cmpw 16(%esp), %ax
    jne 1f
    # Userspace exception
    movl %gs:16, %eax
    popl (%eax)
    movl %ebx, 4(%eax)
    movl %ecx, 8(%eax)
    movl %edx, 12(%eax)
    movl %esi, 16(%eax)
    movl %edi, 20(%eax)
    movl %ebp, 24(%eax)
    
    movl 8(%esp), %ecx # %eip
    movl 16(%esp), %edx # eflags
    movl %ecx, 32(%eax)
    movl %edx, 36(%eax)
    movl 20(%esp), %ecx # esp
    movl 4(%esp), %edx # error code
    movl %ecx, 28(%eax)
    xorl %ecx, %ecx
    movl %ecx, 52(%eax)
    xorl %eax, %eax
    ret
1:
    # Kernel thread exception
    movw $0x70, %ax
    cmpw 8(%esp), %ax
    jne 1f

    movl %gs:16, %eax
    popl (%eax)
    movl %ebx, 4(%eax)
    movl %ecx, 8(%eax)
    movl %edx, 12(%eax)
    movl %esi, 16(%eax)
    movl %edi, 20(%eax)
    movl %ebp, 24(%eax)

    popl %ecx
    popl %edx
    addl $4, %esp
    popl 32(%eax)
    addl $4, %esp
    popl 36(%eax)
    movl %esp, 28(%eax)
    movl %gs:4, %esp
    pushl $0
    pushl %ecx
    movl $4, %ecx
    movl %ecx, 52(%eax)
    xorl %eax, %eax
    ret
1:
    # Kernel exception
    xchgl %ecx, (%esp)
    xchgl %ecx, 4(%esp)
    pushl %edx
    pushl %ebx
    leal 16(%esp), %eax
    pushl %eax
    pushl %ebp
    pushl %esi
    pushl %edi

    movl %esp, %eax
    movl 32(%esp), %edx
    jmp *%ecx


    .type page_fault_isr, @function
    .globl page_fault_isr
page_fault_isr:
    call save_registers

    subl $16, %esp
    andl $0xfffffff0, %esp
    pushl $0
    pushl $0
    movl %esp, %ebp
    pushl %edx
    pushl %eax

    movl %eax, %edi

    call page_fault_handler

    cmpl $0, %edi
    je 1f

    movl %ebp, %esp
    
    popal
    
    iretl

1:
    return_from_kernel_macro

    .type general_protection_fault_isr, @function
    .globl general_protection_fault_isr
general_protection_fault_isr:
    call save_registers

    pushl $0
    pushl $0
    movl %esp, %ebp
    andl $0xfffffff0, %esp
    
    subl $8, %esp
    pushl %edx
    pushl %eax

    movl %eax, %edi

    call general_protection_fault_handler

    cmpl $0, %edi
    je 1f

    movl %ebp, %esp
    addl $8, %esp

    popl %edi
    popl %esi
    popl %ebp
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax
    addl $4, %esp
    iretl

1:
    return_from_kernel_macro

    .type sse_exception_isr, @function
    .globl sse_exception_isr
sse_exception_isr:
    pushl $0 # fake error code
    call save_registers

    pushl $0
    pushl $0
    movl %esp, %ebp
    andl $0xfffffff0, %esp

    call sse_exception_manager

    return_from_kernel_macro