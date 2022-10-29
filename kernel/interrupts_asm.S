/* ------------------------------------------- MACROS --------------------------------------------- */

    .macro save_scratch
    pushq %rax
    movq current_task(%rip), %rax // TODO

    movq %rdi, 0(%rax)
    movq %rsi, 8(%rax)
    movq %rdx, 16(%rax)
    movq %rcx, 24(%rax)
    movq %r8, 32(%rax)
    movq %r9, 40(%rax)
    popq 48(%rax)
    movq %r10, 56(%rax)
    movq %r11, 64(%rax)
    .endm

    .macro restore_scratch
    movq current_task(%rip), %rax
    movq 0(%rax), %rdi
    movq 8(%rax), %rsi
    movq 16(%rax), %rdx
    movq 24(%rax), %rcx
    movq 32(%rax), %r8
    movq 40(%rax), %r9
    movq 56(%rax), %r10
    movq 64(%rax), %r11
    movq 48(%rax), %rax
    .endm

    
    

    .altmacro
    .macro interrupt_err num
    .globl isr\num
    .type isr\num, @function
isr\num\(): \
    push $\num
    jmp ints_handler
    .endm

    .macro interrupt_noerr num
    .globl isr\num
    .type isr\num, @function
isr\num\():
    push $0
    push $\num
    jmp ints_handler
    .endm

    .macro fill funct, from, to
    .local i
    .set i, \from
    .rept \to-\from+1
    \funct %i
    .set i, i+1
    .endr
    .endm

/* ------------------------------------------ ISRs -------------------------------------------------- */

fill interrupt_noerr, 0,7
interrupt_err         8
interrupt_noerr       9
fill interrupt_err,   10,14
fill interrupt_noerr  15,16
interrupt_err         17
fill interrupt_noerr, 18,29
interrupt_err         30
interrupt_noerr       31
fill interrupt_noerr, 32, 255

/* ------------------------------------------ FUNCTIONS ---------------------------------------------- */


    .globl loadIDT
    .type loadIDT, @function
loadIDT:
    lidt (%rdi)
    ret

    .globl mask_PIC
    .type mask_PIC, @function
mask_PIC:
    mov $0xff, %al
    out %al, $0xa1
    out %al, $0x21
    ret

    .globl ints_handler
    .type ints_handler, @function
ints_handler:
    save_scratch
    cmpq $0xCA, 0(%rsp) 
    jne not_syscall

    addq $16, %rsp
    call syscall_handler
    jmp ret_from_interrupt
not_syscall:
    popq %rdi
    popq %rsi
    call interrupt_handler
ret_from_interrupt:
    restore_scratch
    iretq