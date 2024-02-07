    .type setcontext, @function
    .globl setcontext
setcontext:
    movq %rbx, 0(%rdi)
    movq %r12, 8(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 24(%rdi)
    movq %r15, 32(%rdi)

    movq %rsp, 40(%rdi)
    movq %rbp, 48(%rdi)

    # 0(%rbp) is the return address
    # movq 0(%rsp), %rax
    # movq %rax, 40(%rdi)

    # Return address is saved in stack

    # return 0
    subq %rax, %rax

    retq