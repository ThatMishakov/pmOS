    .macro save_regs
    # Save %r15 and load current_task pointer into %r15
    movq %r15, %gs:32
    movq %gs:16, %r15

    # Scratch regs
    movq %rdi, 0(%r15)
    movq %rsi, 8(%r15)
    movq %rdx, 16(%r15)
    movq %rcx, 24(%r15)
    movq %r8, 32(%r15)
    movq %r9, 40(%r15)
    movq %rax, 48(%r15)
    movq %r10, 56(%r15)
    movq %r11, 64(%r15)


    # Preserved regs
    movq %rbx, 112(%r15)
    movq %rbp, 120(%r15)
    movq %r12, 128(%r15)
    movq %r13, 136(%r15)
    movq %r14, 144(%r15)

    movq %gs:32, %rdi
    movq %rdi, 152(%r15)

    # FSBase
    movl $0xC0000100, %ecx
    rdmsr
    movl %eax, 160(%r15)
    movl %edx, 164(%r15)

    # KernelGSBase (actually, it's user's GSBase)
    movl $0xC0000102, %ecx
    rdmsr
    movl %eax, 168(%r15)
    movl %edx, 172(%r15)

    # Stack
    movq %rsp, 96(%r15)

    # Also, %rip can be loaded from %ecx here to preserve compaitability with iret
    .endm

    .macro restore_regs
    movq       %gs:16, %r15

    movq      96(%r15), %rsp


    movl $0xC0000100, %ecx
    movl       160(%r15), %eax
    movl       164(%r15), %edx
    wrmsr

    movl $0xC0000102, %ecx
    movl       168(%r15), %eax
    movl       172(%r15), %edx
    wrmsr

    movq 0(%r15), %rdi
    movq 8(%r15), %rsi
    movq 16(%r15), %rdx
    movq 24(%r15), %rcx
    movq 32(%r15), %r8
    movq 40(%r15), %r9
    movq 48(%r15), %rax
    movq 56(%r15), %r10
    movq 64(%r15), %r11

    movq       112(%r15), %rbx
    movq       120(%r15), %rbp
    movq       128(%r15), %r12
    movq       136(%r15), %r13
    movq       144(%r15), %r14
    movq       152(%r15), %r15
    .endm



    .globl syscall_entry
syscall_entry:

    # SYSCALL will always be from ring 0 to ring 3
    swapgs

    # Save task's registers
    save_regs

    # Entry type
    movq $1,   176(%r15)

    # Switch to kernelss stack
    movq %gs:8, %rsp

    # Stack frame conventions
    movq $0, %rbp

    call syscall_handler

    # Syscall might do a task switch, which might need to be returned to by iret
    # Check that and change return method if that's the case
    movq %gs:16, %r15
    cmpq $1, 176(%r15)
    jne ret_from_interrupt

    .globl ret_from_syscall
ret_from_syscall:
    
    # Restore registers (and also user's stack)
    restore_regs

    # SYSRET will always be from ring 3 to ring 0
    swapgs

    # Return
    sysretq
