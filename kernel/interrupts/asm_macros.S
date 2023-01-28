    .macro save_context_with_intno
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

    popq       192(%r11)
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
    .endm

    .macro save_context
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
    .endm

    .macro save_context_with_error
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
    .endm

    .macro restore_context
    movq       %gs:24, %r11
    pushq      104(%r11)
    pushq      96(%r11)
    pushq      88(%r11)
    pushq      80(%r11)
    pushq      72(%r11)
    movq       112(%r11), %rbx
    movq       120(%r11), %rbp
    movq       128(%r11), %r12
    movq       136(%r11), %r13
    movq       144(%r11), %r14
    movq       152(%r11), %r15

    movq 0(%r11), %rdi
    movq 8(%r11), %rsi
    movq 16(%r11), %rdx
    movq 24(%r11), %rcx
    movq 32(%r11), %r8
    movq 40(%r11), %r9
    movq 48(%r11), %rax
    movq 56(%r11), %r10
    movq 64(%r11), %r11
    .endm
    
    .macro auto_return
    movq %gs:24, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)
    .endm