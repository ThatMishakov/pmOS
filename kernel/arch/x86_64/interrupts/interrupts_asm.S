/* ------------------------------------------- MACROS --------------------------------------------- */
    .include "interrupts/asm_macros.S"

    .altmacro
    .macro prog_int num
    .type prog_int_\num, @function
    .globl prog_int_\num
prog_int_\num:
    swapgs
    call save_context
    movq $0, %rbp
    movq $0, 176(%r11)
    movq $\num , %rdi
    call programmable_interrupt
    auto_return
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

    .section .text
    fill prog_int,48,247



    .type start_first_process, @function
    .globl start_first_process
start_first_process:
    .type ref_from_interrupt, @function
    .globl ret_from_interrupt
ret_from_interrupt:
    restore_context

	swapgs
    
    iretq




/* ------------------------------------------ FUNCTIONS ---------------------------------------------- */

    .text
    .globl loadIDT
    .type loadIDT, @function
loadIDT:
    lidt (%rdi)
    ret



    .text
    .type apic_timer_isr, @function
    .globl apic_timer_isr
apic_timer_isr:

    # Will be when not in kernel
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call timer_interrupt

    auto_return



    .type lvt0_int_isr, @function
    .globl lvt0_int_isr
lvt0_int_isr:
    
    # Kernel is not preemptive
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call lvt0_int_routine

    auto_return



    .type lvt1_int_isr, @function
    .globl lvt1_int_isr
lvt1_int_isr:
    
    # Kernel is not preemptive
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call lvt1_int_routine

    auto_return



    .type dummy_isr, @function
    .globl dummy_isr
dummy_isr:
    iretq

