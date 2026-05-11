
    .text
    .balign 0x1000
    
    .globl fred_ring3_entry
    .type fred_ring3_entry, @function
fred_ring3_entry:
    incq %gs:24

    pushq %rax
    movq %gs:32, %rax

    movq %rdi, 0(%rax)
    movq %rsi, 8(%rax)
    movq %rdx, 16(%rax)
    movq %rcx, 24(%rax)
    movq %r8, 32(%rax)
    movq %r9, 40(%rax)
    popq 48(%rax)
    movq %r10, 56(%rax)
    movq %r11, 64(%rax)

    movq 0(%rsp), %r8
    movq 8(%rsp), %r9
    movq 16(%rsp), %r10
    movq 24(%rsp), %r11
    movq 32(%rsp), %rdi
    movq 40(%rsp), %rsi

    movq %r8, 184(%rax)
    movq %r9, 72(%rax)
    movq %r10, 80(%rax)
    movq %r11, 88(%rax)
    movq %rdi, 96(%rax)
    movq %rsi, 104(%rax)

    movq %rbx, 112(%rax)
    movq %rbp, 120(%rax)
    movq %r12, 128(%rax)
    movq %r13, 136(%rax)
    movq %r14, 144(%rax)
    movq %r15, 152(%rax)

    xorl %ebp, %ebp

    xorl %edi, %edi
    movq %rsp, %rsi

    call fred_handle

    movq %gs:32, %rax
    movq 176(%rax), %rcx
    leaq fred_return_table(%rip), %rdx
    jmp *(%rdx, %rcx, 8)



    .balign 256, 0xcc
    .type fred_kernel_entry, @function
    .globl fred_kernel_entry
fred_kernel_entry:
    cmpq $0, %gs:24
    je kernel_thread_interrupt

    incq %gs:24

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    // Alignment
    pushq $0

    xorl %ebp, %ebp

    # Maybe kernel context...
    leaq 8(%rsp), %rdi
    # Fred context
    leaq 128(%rsp), %rsi
    
    call fred_handle

    addq $8, %rsp
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    decq %gs:24

    erets


kernel_thread_interrupt:
    incq %gs:24

    pushq %rax
    movq %gs:32, %rax

    movq %rdi, 0(%rax)
    movq %rsi, 8(%rax)
    movq %rdx, 16(%rax)
    movq %rcx, 24(%rax)
    movq %r8, 32(%rax)
    movq %r9, 40(%rax)
    popq 48(%rax)
    movq %r10, 56(%rax)
    movq %r11, 64(%rax)

    movq 0(%rsp), %r8
    movq 8(%rsp), %r9
    movq 16(%rsp), %r10
    movq 24(%rsp), %r11
    movq 32(%rsp), %rdi
    movq 40(%rsp), %rsi

    movq %r8, 184(%rax)
    movq %r9, 72(%rax)
    movq %r10, 80(%rax)
    movq %r11, 88(%rax)
    movq %rdi, 96(%rax)
    movq %rsi, 104(%rax)

    movq %rbx, 112(%rax)
    movq %rbp, 120(%rax)
    movq %r12, 128(%rax)
    movq %r13, 136(%rax)
    movq %r14, 144(%rax)
    movq %r15, 152(%rax)

    xorl %ebp, %ebp

    xorl %edi, %edi
    movq %rsp, %rsi

    movq %gs:8, %rsp

    call fred_handle

    .globl fred_return
fred_return:
    movq %gs:32, %rax
    movq 176(%rax), %rcx
    leaq fred_return_table(%rip), %rdx
    jmp *(%rdx, %rcx, 8)


    .type fred_ring3_return, @function
fred_ring3_return: 

    movq 0(%rax), %rdi
    movq 8(%rax), %rsi
    movq 16(%rax), %rdx
    movq 24(%rax), %rcx
    movq 32(%rax), %r8
    movq 40(%rax), %r9
    movq 56(%rax), %r10
    movq 64(%rax), %r11

    movq 112(%rax), %rbx
    movq 120(%rax), %rbp
    movq 128(%rax), %r12
    movq 136(%rax), %r13
    movq 144(%rax), %r14
    movq 152(%rax), %r15

    leaq 64(%rax), %rsp
    movq 48(%rax), %rax

    decq %gs:24

    eretu

fred_kernel_thread_return:

    movq 0(%rax), %rdi
    movq 8(%rax), %rsi
    movq 16(%rax), %rdx
    movq 24(%rax), %rcx
    movq 32(%rax), %r8
    movq 40(%rax), %r9
    movq 56(%rax), %r10
    movq 64(%rax), %r11

    movq 112(%rax), %rbx
    movq 120(%rax), %rbp
    movq 128(%rax), %r12
    movq 136(%rax), %r13
    movq 144(%rax), %r14
    movq 152(%rax), %r15

    leaq 64(%rax), %rsp
    movq 48(%rax), %rax

    decq %gs:24

    erets

    .type repeated_syscall_wrapper, @function
repeated_syscall_wrapper:
    # Load current_task into %r9
    movq %gs:32, %r9

    # Restore entry_mode
    movq 200(%r9), %rax
    movq %rax, 176(%r9)
    movq $0, 200(%r9)

    call fred_nested_syscall

    movq %gs:32, %rax
    movq 176(%rax), %rcx
    leaq fred_return_table(%rip), %rdx
    jmp *(%rdx, %rcx, 8)



    .data
    .balign 8
fred_return_table:
.quad fred_ring3_return
.quad fred_kernel_thread_return
.quad 0
.quad repeated_syscall_wrapper