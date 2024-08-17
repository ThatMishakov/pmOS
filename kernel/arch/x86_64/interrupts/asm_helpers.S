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

    .text
    
    .type save_context, @function
    .globl save_context
save_context:
    pushq %r11
    movq %gs:32, %r11

    movq %rdi, 0(%r11)
    movq %rsi, 8(%r11)
    movq %rdx, 16(%r11)
    movq %rcx, 24(%r11)
    movq %r8, 32(%r11)
    movq %r9, 40(%r11)
    movq %rax, 48(%r11)
    movq %r10, 56(%r11)
    popq 64(%r11)

    popq %rax # Return address

    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    jmpq          *%rax



    .type cond_save_context, @function
    .globl cond_save_context
cond_save_context:
    pushq %r11
    movq %gs:32, %r11

    # Save to different place if nested
    cmpw $0x08, 24(%rsp)
	jne 1f
	movq %gs:0, %r11
    leaq 64(%r11), %r11
    addq $1, %gs:24
1:


    movq %rdi, 0(%r11)
    movq %rsi, 8(%r11)
    movq %rdx, 16(%r11)
    movq %rcx, 24(%r11)
    movq %r8, 32(%r11)
    movq %r9, 40(%r11)
    movq %rax, 48(%r11)
    movq %r10, 56(%r11)
    popq 64(%r11)

    popq %rax # Return address

    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    jmpq          *%rax



    .type save_context_with_error, @function
    .globl save_context_with_error
save_context_with_error:
    pushq %r11
    movq %gs:32, %r11

    movq %rdi, 0(%r11)
    movq %rsi, 8(%r11)
    movq %rdx, 16(%r11)
    movq %rcx, 24(%r11)
    movq %r8, 32(%r11)
    movq %r9, 40(%r11)
    movq %rax, 48(%r11)
    movq %r10, 56(%r11)
    popq 64(%r11)

    popq       %rax # Return address
    popq       184(%r11)


    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    jmpq          *%rax



    .type cond_save_context_with_error, @function
    .globl cond_save_context_with_error
cond_save_context_with_error:
    pushq %r11
    movq %gs:32, %r11

    # Save to different place if nested
    cmpw $0x08, 32(%rsp)
	jne 1f
	movq %gs:0, %r11
    leaq 64(%r11), %r11
    addq $1, %gs:24
1:

    movq %rdi, 0(%r11)
    movq %rsi, 8(%r11)
    movq %rdx, 16(%r11)
    movq %rcx, 24(%r11)
    movq %r8, 32(%r11)
    movq %r9, 40(%r11)
    movq %rax, 48(%r11)
    movq %r10, 56(%r11)
    popq 64(%r11)

    popq       %rax # Return address
    popq       184(%r11)


    popq       72(%r11)
    popq       80(%r11)
    popq       88(%r11)
    popq       96(%r11)
    popq       104(%r11)
    movq %rbx, 112(%r11)
    movq %rbp, 120(%r11)
    movq %r12, 128(%r11)
    movq %r13, 136(%r11)
    movq %r14, 144(%r11)
    movq %r15, 152(%r11)
    jmpq           *%rax




    .type jumpto_func, @function
    .globl jumpto_func
jumpto_func:
    pushq  %gs:48
    jmpq  *%gs:56


    .type dbg_saveregs, @function
    .globl dbg_saveregs
dbg_saveregs:
    movq    %rdi, kernel_interrupt_regs + 0(%rip)
    movq    %rsi, kernel_interrupt_regs + 8(%rip)
    movq    %rdx, kernel_interrupt_regs + 16(%rip)
    movq    %rcx, kernel_interrupt_regs + 24(%rip)
    movq    %r8, kernel_interrupt_regs + 32(%rip)
    movq    %r9, kernel_interrupt_regs + 40(%rip)
    movq    %rax, kernel_interrupt_regs + 48(%rip)
    movq    %r10, kernel_interrupt_regs + 56(%rip)
    movq    %r11, kernel_interrupt_regs + 64(%rip)
    movq 16(%rsp), %rax # %rip
    movq %rax, kernel_interrupt_regs + 72(%rip)
    movq 32(%rsp), %rax # rflags
    movq %rax, kernel_interrupt_regs + 88(%rip)
    movq 40(%rsp), %rax # %rsp
    movq %rax, kernel_interrupt_regs + 96(%rip)
    movq 8(%rsp), %rax # exception number
    movq %rax, kernel_interrupt_regs + 184(%rip)
    movq %rbx, kernel_interrupt_regs + 112(%rip)
    movq %rbp, kernel_interrupt_regs + 120(%rip)
    movq %r12, kernel_interrupt_regs + 128(%rip)
    movq %r13, kernel_interrupt_regs + 136(%rip)
    movq %r14, kernel_interrupt_regs + 144(%rip)
    movq %r15, kernel_interrupt_regs + 152(%rip)
    retq

    .type dbg_loadregs, @function
    .globl dbg_loadregs
dbg_loadregs:
    movq kernel_interrupt_regs + 0(%rip), %rdi
    movq kernel_interrupt_regs + 8(%rip), %rsi
    movq kernel_interrupt_regs + 16(%rip), %rdx
    movq kernel_interrupt_regs + 24(%rip), %rcx
    movq kernel_interrupt_regs + 32(%rip), %r8
    movq kernel_interrupt_regs + 40(%rip), %r9
    movq kernel_interrupt_regs + 48(%rip), %rax
    movq kernel_interrupt_regs + 56(%rip), %r10
    movq kernel_interrupt_regs + 64(%rip), %r11
    movq kernel_interrupt_regs + 112(%rip), %rbx
    movq kernel_interrupt_regs + 120(%rip), %rbp
    movq kernel_interrupt_regs + 128(%rip), %r12
    movq kernel_interrupt_regs + 136(%rip), %r13
    movq kernel_interrupt_regs + 144(%rip), %r14
    movq kernel_interrupt_regs + 152(%rip), %r15
    retq