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

    .macro auto_return
    movq %gs:32, %r15
    movq 176(%r15), %r15
    leaq return_table(%rip), %r14
    jmp *(%r14,%r15, 8)
    .endm

    .text
    .type save_context, @function
    .globl save_context
save_context:
    cmpw $0x08, 24(%rsp)
    je 1f

    pushq %rax
    movq %gs:32, %rax

    popq 48(%rax)
    movq %rdi, 0(%rax)
    movq %rsi, 8(%rax)
    movq %rdx, 16(%rax)
    movq %rcx, 24(%rax)
    movq %r8, 32(%rax)
    movq %r9, 40(%rax)
    movq %r10, 56(%rax)
    movq %r11, 64(%rax)

    popq %rcx # Return address
    popq %rdx # Error code

    popq 72(%rax)
    popq 80(%rax)
    popq 88(%rax)
    popq 96(%rax)
    popq 104(%rax)
    
    movq %rbx, 112(%rax)
    movq %rbp, 120(%rax)
    movq %r12, 128(%rax)
    movq %r13, 136(%rax)
    movq %r14, 144(%rax)
    movq %r15, 152(%rax)

    cmpw $0x58, 80(%rax)
    jne 2f
    # Switch to kernel stack
    movq %gs:8, %rsp

    # Entry type
    movq $4,   176(%rax)
    xorq %rax, %rax
    jmpq *%rcx
2:
    movq $0,   176(%rax)
    xorq %rax, %rax
    jmpq *%rcx
1:
# Kernel exception
    xchgq %r15, 8(%rsp) # Error code
    xchgq %r14, 0(%rsp) # Return address
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    
    movq %r15, %rdx
    movq %rsp, %rax
    jmpq *%r14

    .type return_to_kernel, @function
return_to_kernel:
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    iretq

    # TODO
    .type breakpoint_isr, @function
    .globl breakpoint_isr
breakpoint_isr:

    cmpw $0x08, 8(%rsp)
    je 1f

    # It will always be from userspace to ring 0
    swapgs
1:

    pushq $0

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp
    movq %rax, %r12
    movq %rax, %rdi
    movq %rdx, %rsi

    call breakpoint_manager

    cmpq $0, %r12
    jne 1f
    auto_return
1:
    jmp return_to_kernel


    .text
    .type sse_exception_isr, @function
    .globl sse_exception_isr
sse_exception_isr:

    # It will always be from userspace to ring 0
    swapgs

    # Push fake error code
    pushq $0

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp

    call sse_exception_manager

    auto_return



    .type invalid_opcode_isr, @function
    .globl invalid_opcode_isr
invalid_opcode_isr:

    # It will always be from userspace to ring 0
    swapgs

    pushq $0

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp

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
    call save_context

    # Calling conventions
    movq $0, %rbp
    movq %rax, %r12
    movq %rax, %rdi
    movq %rdx, %rsi
    call general_protection_fault_manager

    cmpq $0, %r12
    jne 1f
    # Return
    auto_return
1:
    jmp return_to_kernel



    .type pagefault_isr, @function
    .globl pagefault_isr
pagefault_isr:

    # Jump to debugger upon pagefault in kernel
    cmpw $0x08, 16(%rsp)
	je 1f
	swapgs
1:

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp
    movq %rax, %r12
    movq %rax, %rdi
    movq %rdx, %rsi
    call pagefault_manager

    cmpq $0, %r12
    jne 1f

    # Return
    auto_return

1:
    jmp return_to_kernel


    # TODO
    .type stack_segment_fault_isr, @function
    .globl stack_segment_fault_isr
stack_segment_fault_isr:

    # swapgs conditionally
    cmpw $0x08, 16(%rsp)
	je 1f
	swapgs
1:

    # Save registers
    call save_context

    # Calling conventions
    movq $0, %rbp
    movq %rax, %r12
    movq %rax, %rdi
    movq %rdx, %rsi
    call stack_segment_fault_manager

    cmpq $0, %r12
    jne 1f
    # Return
    auto_return
1:
    jmp return_to_kernel


    .type overflow_isr, @function
    .globl overflow_isr
overflow_isr:
    swapgs

    pushq $0
    call save_context

    movq $0, %rbp

    call overflow_manager

    auto_return

    .type simd_fp_exception_isr, @function
    .globl simd_fp_exception_isr
simd_fp_exception_isr:
    swapgs

    pushq $0
    call save_context

    movq $0, %rbp

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
    call save_context

    # Calling conventions
    movq $0, %rbp
    movq %rax, %r12
    movq %rax, %rdi
    movq %rdx, %rsi

    call double_fault_manager

    cmpq $0, %r12
    jne 1f
    # Return
    auto_return
1:
    jmp return_to_kernel
