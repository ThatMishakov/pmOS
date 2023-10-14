    .type __ucontext_start, @function
    .globl __ucontext_start
    .hidden __ucontext_start
__ucontext_start:
    # Since the saved registers are what's restored, use them
    # to pass initial arguments to the function.
    movq %rbx, %rdi
    movq %r12, %rsi
    movq %r13, %rdx
    movq %r14, %rcx
    movq %r15, %r8
    movq %rbp, %r9

    # POSIX explicitly says that arguments have to be integers
    # so don't worry about XMM stuff

    # Restore %rbp (to 0)
    movq $0, %rbp

    # Save the uc_link pointer to preserved register
    popq %rbx

    # Call the function
    # makecontext should have left the function address on the stack
    # with the right alignment after popping it
    popq %rax
    callq *%rax

    # I've seen implementations that leave cleanup function address
    # as the return address, but it seems to leave the stack unaligned
    # (which is not up to System V ABI and leads to problems)
    # so either more assembly magic is needed or beter off just
    # call it here as normal function

    # Pass the uc_link pointer as the first argument
    movq %rbx, %rdi
    # Call the cleanup function
    call __ucontext_cleanup

    # Should never get here
    ud2