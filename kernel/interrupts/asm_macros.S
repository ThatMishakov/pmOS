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



    .macro restore_context_nested
    subq       $1, %gs:40

    movq       %gs:0, %r11
    leaq       64(%r11), %r11

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
    
    .macro auto_return_cond
    cmpq $0, %gs:40
    jne  ret_nested_exception
    movq %gs:24, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)
    .endm


    .macro auto_return
    movq %gs:24, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)
    .endm

    # Macro indicating code needed for calling mini debugger
    # intno indicated the exception vector
    # errcode indicates whether the exception pushes an error code. 0 means it doesn't
    .macro dbg_code intno, errcode
    .if \errcode == 0
    pushq   $0
    .endif
    call dbg_saveregs
    addq $8, %rsp
    movq $\intno, %rdi
    call dbg_main
    movq    dbg_regs + 31*8, %rax   # update the return address
    movq    %rax, (%rsp)
    call dbg_loadregs
    iretq
    .endm