    .type swapcontext, @function
    .globl swapcontext
swapcontext:
    # Save the current context
    movq %rbx, 0(%rdi)
    movq %r12, 8(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 24(%rdi)
    movq %r15, 32(%rdi)

    movq %rsp, 40(%rdi)
    movq %rbp, 48(%rdi)

    # Restore the new context
    movq 0(%rsi), %rbx
    movq 8(%rsi), %r12
    movq 16(%rsi), %r13
    movq 24(%rsi), %r14
    movq 32(%rsi), %r15

    movq 40(%rsi), %rsp
    movq 48(%rsi), %rbp

    # Set the return value to 0
    subq %rax, %rax

    # Return to the new context
    ret