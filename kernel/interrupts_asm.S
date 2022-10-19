/* ------------------------------------------- MACROS --------------------------------------------- */

#define PUSHALL \
    push %rax; \
    push %rbx; \
    push %rcx; \
    push %rdx; \
    push %rsi; \
    push %rdi; \
    push %rbp; \
    push %r8; \
    push %r9; \
    push %r10; \
    push %r11; \
    push %r12; \
    push %r13; \
    push %r14; \
    push %r15;

#define POPALL \
    pop %r15; \
    pop %r14; \
    pop %r13; \
    pop %r12; \
    pop %r11; \
    pop %r10; \
    pop %r9;  \
    pop %r8;  \
    pop %rbp; \
    pop %rdi; \
    pop %rsi; \
    pop %rdx; \
    pop %rcx; \
    pop %rbx; \
    pop %rax;

#define SAVE_INT_FRAME \
    push %rax; \
    movq current_task(%rip), %rax; \
    movq %r15, 0(%rax); \
    movq %r14, 8(%rax); \
    movq %r13, 16(%rax); \
    movq %r12, 24(%rax); \
    movq %r11, 32(%rax); \
    movq %r10, 40(%rax); \
    movq %r9, 48(%rax); \
    movq %r8, 56(%rax); \
    movq %rbp, 64(%rax); \
    movq %rdi, 72(%rax); \
    movq %rsi, 80(%rax); \
    movq %rdx, 88(%rax); \
    movq %rcx, 96(%rax); \
    movq %rbx, 104(%rax); \
    popq 112(%rax); /* %rax */\
    popq 120(%rax); /* intno */\
    popq 128(%rax); /* err */\
    popq 136(%rax); /* rip */\
    popq 144(%rax); /* cs */\
    popq 152(%rax); /* rflags */\
    popq 160(%rax); /* rsp */
    popq 168(%rax); /* ss */

#define RESTORE_INT_FRAME \
    movq current_task(%rip), %rax; \
    movq 0(%rax), %r15; \
    movq 8(%rax), %r14; \
    movq 16(%rax), %r13; \
    movq 24(%rax), %r12; \
    movq 32(%rax), %r11; \
    movq 40(%rax), %r10; \
    movq 48(%rax), %r9; \
    movq 56(%rax), %r8; \
    movq 64(%rax), %rbp; \
    movq 72(%rax), %rdi; \
    movq 80(%rax), %rsi; \
    movq 88(%rax), %rdx; \
    movq 96(%rax), %rcx; \
    movq 104(%rax), %rbx; \
    pushq 168(%rax); /* ss */ \
    pushq 160(%rax); /* rsp */ \
    pushq 152(%rax); /* rflags */ \
    pushq 144(%rax); /* cs */ \
    pushq 136(%rax); /* rip */ \
    movq 112(%rax), %rax /* Restore %rax */
    
    
    


#define INTERRUPT_ERR(NUM) \
    .globl isr##NUM; \
    .type isr##NUM, @function; \
isr##NUM: \
    push $NUM; \
    jmp ints_handler; \

#define INTERRUPT_NOERR(NUM) \
    .globl isr##NUM; \
    .type isr##NUM, @function; \
isr##NUM: \
    push $0; \
    push $NUM; \
    jmp ints_handler; \

/* ------------------------------------------ ISRs -------------------------------------------------- */

INTERRUPT_NOERR(0)
INTERRUPT_NOERR(1)
INTERRUPT_NOERR(2)
INTERRUPT_NOERR(3)
INTERRUPT_NOERR(4)
INTERRUPT_NOERR(5)
INTERRUPT_NOERR(6)
INTERRUPT_NOERR(7)
INTERRUPT_ERR  (8)
INTERRUPT_NOERR(9)
INTERRUPT_ERR  (10)
INTERRUPT_ERR  (11)
INTERRUPT_ERR  (12)
INTERRUPT_ERR  (13)
INTERRUPT_ERR  (14)
INTERRUPT_NOERR(15)
INTERRUPT_NOERR(16)
INTERRUPT_ERR  (17)
INTERRUPT_NOERR(18)
INTERRUPT_NOERR(19)
INTERRUPT_NOERR(20)
INTERRUPT_NOERR(21)
INTERRUPT_NOERR(22)
INTERRUPT_NOERR(23)
INTERRUPT_NOERR(24)
INTERRUPT_NOERR(25)
INTERRUPT_NOERR(26)
INTERRUPT_NOERR(27)
INTERRUPT_NOERR(28)
INTERRUPT_NOERR(29)
INTERRUPT_ERR  (30)
INTERRUPT_NOERR(31)

INTERRUPT_NOERR(32)
INTERRUPT_NOERR(33)
INTERRUPT_NOERR(34)
INTERRUPT_NOERR(35)
INTERRUPT_NOERR(36)
INTERRUPT_NOERR(37)
INTERRUPT_NOERR(38)
INTERRUPT_NOERR(39)
INTERRUPT_NOERR(40)
INTERRUPT_NOERR(41)
INTERRUPT_NOERR(42)
INTERRUPT_NOERR(43)
INTERRUPT_NOERR(44)
INTERRUPT_NOERR(45)
INTERRUPT_NOERR(46)
INTERRUPT_NOERR(47)

INTERRUPT_NOERR(127)
INTERRUPT_NOERR(128)

INTERRUPT_NOERR(202)

/* ------------------------------------------ FUNCTIONS ---------------------------------------------- */


    .globl loadIDT
    .type loadIDT, @function
loadIDT:
    lidt (%rdi)
    sti
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
    cld
    SAVE_INT_FRAME
    call interrupt_handler
    RESTORE_INT_FRAME
    iretq