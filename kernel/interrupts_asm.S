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
    PUSHALL
    mov %rsp, %rdi
    call interrupt_handler
    mov %rax, %rsp
    POPALL
    add 16, %rsp // skip errcode and int no
    iretq
