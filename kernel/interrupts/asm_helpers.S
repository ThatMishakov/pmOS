    .text
    
    .type save_context, @function
    .globl save_context
save_context:
    pushq %r11
    movq %gs:24, %r11

    movq %rdi, 0(%r11)
    movq %rsi, 8(%r11)
    movq %rdx, 16(%r11)
    movq %rcx, 24(%r11)
    movq %r8, 32(%r11)
    movq %r9, 40(%r11)
    movq %rax, 48(%r11)
    movq %r10, 56(%r11)
    popq 64(%r11)

    popq %rax # Return address

    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    jmpq %rax



    .type save_context_with_error, @function
    .globl save_context_with_error
save_context_with_error:
    pushq %r11
    movq %gs:24, %r11

    movq %rdi, 0(%r11)
    movq %rsi, 8(%r11)
    movq %rdx, 16(%r11)
    movq %rcx, 24(%r11)
    movq %r8, 32(%r11)
    movq %r9, 40(%r11)
    movq %rax, 48(%r11)
    movq %r10, 56(%r11)
    popq 64(%r11)

    popq       %rax # Return address
    popq       184(%r11)

    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    jmpq           %rax
