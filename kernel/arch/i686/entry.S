    .text
    .globl return_from_kernel
return_from_kernel:
    movl %gs:16, %eax
    movl 52(%eax), %edx # Entry type
    jmp *return_table(,%edx,4)


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
    popl 44(%eax) # ss

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