    .include "interrupts/asm_macros.S"


    .type breakpoint_isr, @function
    .globl breakpoint_isr
breakpoint_isr:

    # Invoke debuger if breakpoint was hit in kernel
    cmpw $0x08, 8(%rsp)
    je 1f

    # It will always be from userspace to ring 0
    swapgs

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call breakpoint_manager

    auto_return

1:
    dbg_code 3, 0



    .text
    .type sse_exception_isr, @function
    .globl sse_exception_isr
sse_exception_isr:

    # It will always be from userspace to ring 0
    swapgs

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call sse_exception_manager

    auto_return



    .type invalid_opcode_isr, @function
    .globl invalid_opcode_isr
invalid_opcode_isr:

    # It will always be from userspace to ring 0
    swapgs

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call invalid_opcode_manager

    auto_return



    .type general_protection_fault_isr, @function
    .globl general_protection_fault_isr
general_protection_fault_isr:

    # swapgs conditionally
    cmpw $0x08, 16(%rsp)
	je 1f
	swapgs
1:

    # Save registers
    call cond_save_context_with_error

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call general_protection_fault_manager

    # Return
    auto_return_cond



    .type pagefault_isr, @function
    .globl pagefault_isr
pagefault_isr:

    # Jump to debugger upon pagefault in kernel
    cmpw $0x08, 16(%rsp)
	je 1f
	swapgs

    # Save registers
    call cond_save_context_with_error

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call pagefault_manager

    # Return
    auto_return_cond

1:
    dbg_code 14, 1


    .type stack_segment_fault_isr, @function
    .globl stack_segment_fault_isr
stack_segment_fault_isr:

    # swapgs conditionally
    cmpw $0x08, 16(%rsp)
	je 1f
	swapgs
1:

    # Save registers
    call cond_save_context_with_error

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call stack_segment_fault_manager

    # Return
    auto_return_cond



    .type double_fault_isr, @function
    .globl double_fault_isr
double_fault_isr:

    # swapgs conditionally
    cmpw $0x08, 16(%rsp)
	je 1f
	swapgs
1:

    # Save registers
    call cond_save_context_with_error

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call double_fault_manager

    # Return
    auto_return_cond


    .type ret_nested_exception, @function
    .globl ret_nested_exception
ret_nested_exception:
    restore_context_nested

    iretq
