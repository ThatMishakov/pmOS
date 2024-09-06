# Copyright (c) 2024, Mikhail Kovalev
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

    # Save registers
    call cond_save_context_with_error

    # Calling conventions
    movq $0, %rbp

    # Entry type
    movq $0,   176(%r11)

    call general_protection_fault_manager

    # Return
    auto_return_cond

1:
    dbg_code 13, 1



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


    .type overflow_isr, @function
    .globl overflow_isr
overflow_isr:
    swapgs

    call save_context

    movq $0, %rbp
    movq $0,   176(%r11)

    call overflow_manager

    auto_return

    .type simd_fp_exception_isr, @function
    .globl simd_fp_exception_isr
simd_fp_exception_isr:
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call simd_fp_exception_manager

    auto_return


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
