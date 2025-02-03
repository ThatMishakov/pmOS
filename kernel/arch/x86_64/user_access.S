    .text
    .balign 16, 0x90
    .type fast_copy_to_user, @function
    .globl fast_copy_to_user
fast_copy_to_user:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    pushq $2
    jmp generic_copy

    .type fast_copy_from_user, @function
    .globl fast_copy_from_user
fast_copy_from_user:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    pushq $1
generic_copy:
    pushq %rdx # size_t size
    pushq %rsi # const void *src
    pushq %rdi # void *dst

    pushq $0 # alignment

    movq %rsp, %gs:56
    leaq on_failure(%rip), %rax
    movq %rax, %gs:48

    call memcpy

    xorq %rax, %rax
    movq $1, %rdx
    movq %rax, %gs:48

    movq %rbp, %rsp
    popq %rbp
    retq

on_failure:
    movq %rax, %rsp
    leaq 80(%rsp), %rbp

    movq %rcx,      %rsi # const void *faulting_addr
    leaq -56(%rbp), %r8  # size_t size
    leaq -64(%rbp), %rcx # const void *src
    leaq -72(%rbp), %rdx # void *dst
    movq -48(%rbp), %rdi # unsigned access

    call user_access_page_fault

    cmpq $0, %rax
    jne failure
    cmpq $1, %rdx
    jne failure

    movq -56(%rbp), %rdx
    movq -64(%rbp), %rsi
    movq -72(%rbp), %rdi

    call memcpy

    movq $1, %rdx
    xorq %rax, %rax
failure:
    xorq %rcx, %rcx
    movq %rcx, %gs:48

    addq $40, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    retq

