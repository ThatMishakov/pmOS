/* ------------------------------------------- MACROS --------------------------------------------- */
    .include "interrupts/asm_macros.S"

    .altmacro
    .macro interrupt_err num
    .type isr\num, @function
isr\num\(): \
    push $\num
    jmp ints_handler
    .endm

    .macro interrupt_noerr num
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


#define ISR 0x01
#define INTGATE 0x8e
#define TRAPGATE 0xee

    .macro init_intgate isr_n
    leaq isr\isr_n(%rip), %rdi
    leaq (16*\isr_n) + k_idt(%rip), %rsi
    call set_gate_desc_int
    .endm

    .macro init_trapgate isr_n
    leaq isr\isr_n(%rip), %rdi
    leaq (16*\isr_n) + k_idt(%rip), %rsi
    call set_gate_desc_trap
    .endm

/* ------------------------------------------ ISRs -------------------------------------------------- */

    .section .data
    fill interrupt_noerr, 0,7
    interrupt_err         8
    interrupt_noerr       9
    fill interrupt_err,   10,14
    fill interrupt_noerr  15,16
    interrupt_err         17
    fill interrupt_noerr, 18,29
    interrupt_err         30
    interrupt_noerr       31
    fill interrupt_noerr, 32, 251
    interrupt_noerr       252
    fill interrupt_noerr, 253, 255



/* ------------------------------------------ IDT --------------------------------------------------- */


    .section .data
    .type set_idt_e, @function
set_gate_desc_int:
    movw %di, 0(%rsi)
    movw $0x08, 2(%rsi)
    movw $(ISR | (INTGATE << 8)), 4(%rsi)
    shrq $16, %rdi
    movw %di, 6(%rsi)
    shrq $16, %rdi
    movl %edi, 8(%rsi)
    movl $0, 12(%rsi)
    ret

    .section .data
    .type set_idt_e, @function
set_gate_desc_trap:
    movw %di, 0(%rsi)
    movw $0x08, 2(%rsi)
    movw $(ISR | (TRAPGATE << 8)), 4(%rsi)
    shrq $16, %rdi
    movw %di, 6(%rsi)
    shrq $16, %rdi
    movl %edi, 8(%rsi)
    movl $0, 12(%rsi)
    ret

    .section .data
    .globl fill_idt
    .type fill_idt, @function
fill_idt:
    pushq %rbp
    movq %rsp, %rbp
    init_intgate 0
    init_trapgate 1
    init_intgate 2
    init_trapgate 3
    init_trapgate 4
    fill init_intgate, 5, 201
    init_trapgate 202
    fill init_intgate 203, 255
    //movq 0(%rbp), %rsp
    popq %rbp
    ret



/* ------------------------------------------ FUNCTIONS ---------------------------------------------- */


    .globl loadIDT
    .type loadIDT, @function
loadIDT:
    lidt (%rdi)
    ret

    .globl ints_handler
    .type ints_handler, @function
ints_handler:
    cmpw $0x08, 24(%rsp)
	je 1f
	swapgs
1:
    save_context_with_intno
    movq %gs:24, %rax
    movq $0,   176(%rax) // Entry type

    # Stack frame conventions
    movq $0, %rbp

    call interrupt_handler

    movq %gs:24, %r15
    movq 176(%r15), %r15
    jmpq *return_table(,%r15, 8)

    .globl ret_from_interrupt
ret_from_interrupt:
    .globl start_first_process
start_first_process:
    restore_context

    cmpw $0x08, 0x8(%rsp)
	je 1f
	swapgs
1:
    iretq



    .text
    .type apic_timer_isr, @function
    .globl apic_timer_isr
apic_timer_isr:

    # Will be when not in kernel
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call sched_periodic

    auto_return

