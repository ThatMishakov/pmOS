    .include "interrupts/asm_macros.S"

    .type ipi_reschedule_isr, @function
    .globl ipi_reschedule_isr
ipi_reschedule_isr:
    
    # Kernel is not preemptive
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call reschedule

    auto_return



    .type ipi_invalidate_tlb_isr, @function
    .globl ipi_invalidate_tlb_isr
ipi_invalidate_tlb_isr:
    
    # Kernel is not preemptive
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call ipi_invalidate_tlb_routine

    auto_return
