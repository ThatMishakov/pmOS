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


    .type apic_spurious_isr, @function
    .globl apic_spurious_isr
apic_spurious_isr:
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call apic_spurious_int_routine

    auto_return

    .type apic_dummy_isr, @function
    .globl apic_dummy_isr
apic_dummy_isr:
    swapgs

    call save_context

    movq $0, %rbp
    movq $0, 176(%r11)

    call apic_dummy_int_routine

    auto_return