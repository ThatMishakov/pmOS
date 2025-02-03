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

/* ------------------------------------------- MACROS --------------------------------------------- */

    .macro auto_return
    movq %gs:32, %r15
    movq 176(%r15), %r15
    leaq return_table(%rip), %r14
    jmp *(%r14,%r15, 8)
    .endm

    .altmacro
    .macro prog_int num
    .type prog_int_\num, @function
    .globl prog_int_\num
prog_int_\num:
    pushq $\num
    jmp prog_int_common
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
    .type return_from_kernel_thread, @function
    .globl return_from_kernel_thread
return_from_kernel_thread:
    movq %gs:32, %r15
    pushq      104(%r15)
    pushq      96(%r15)
    pushq      88(%r15)
    pushq      80(%r15)
    pushq      72(%r15)
    movq       112(%r15), %rbx
    movq       120(%r15), %rbp
    movq       128(%r15), %r12
    movq       136(%r15), %r13
    movq       144(%r15), %r14

    movq 0(%r15), %rdi
    movq 8(%r15), %rsi
    movq 16(%r15), %rdx
    movq 24(%r15), %rcx
    movq 32(%r15), %r8
    movq 40(%r15), %r9
    movq 48(%r15), %rax
    movq 56(%r15), %r10
    movq 64(%r15), %r11
    movq 152(%r15), %r15

	swapgs
    
    iretq


    .type prog_int_common, @function
prog_int_common:
    swapgs
    call save_context
    movq %rdx, %rdi

    movq $0, %rbp
    call programmable_interrupt
    auto_return

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

    pushq $0
    call save_context

    movq $0, %rbp

    call timer_interrupt

    auto_return



    .type lvt0_int_isr, @function
    .globl lvt0_int_isr
lvt0_int_isr:
    
    # Kernel is not preemptive
    swapgs

    pushq $0
    call save_context

    movq $0, %rbp

    call lvt0_int_routine

    auto_return



    .type lvt1_int_isr, @function
    .globl lvt1_int_isr
lvt1_int_isr:
    
    # Kernel is not preemptive
    swapgs

    pushq $0
    call save_context

    movq $0, %rbp

    call lvt1_int_routine

    auto_return



    .type dummy_isr, @function
    .globl dummy_isr
dummy_isr:
    iretq


    .type apic_spurious_isr, @function
    .globl apic_spurious_isr
apic_spurious_isr:
    swapgs

    pushq $0
    call save_context

    movq $0, %rbp

    call apic_spurious_int_routine

    auto_return

    .type apic_dummy_isr, @function
    .globl apic_dummy_isr
apic_dummy_isr:
    swapgs

    pushq $0
    call save_context

    movq $0, %rbp

    call apic_dummy_int_routine

    auto_return