    .type getcontext, @function
    .globl getcontext
getcontext:
    movq 0(%rdi), %rbx
    movq 8(%rdi), %r12
    movq 16(%rdi), %r13
    movq 24(%rdi), %r14
    movq 32(%rdi), %r15

    movq 48(%rdi), %rbp
    movq 40(%rdi), %rsp

    # Set the return value to 0
    subq %rax, %rax

    # The stack should have the pointer to the return address
    # so ret should bring us back to the caller
    ret