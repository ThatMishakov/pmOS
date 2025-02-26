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

#define STACK_SIZE 0x4000

    .section .bss
    .align 16
    .globl stack
    .type stack, @object
stack:
    .space STACK_SIZE

    .section .text
    .globl _limine_entry
    .type _limine_entry, @function
_limine_entry:
    # Switch to in-kernel stack, since Limine can't be used because
    # of how how the paging is set up by the kernel
    leaq (stack+STACK_SIZE)(%rip), %rsp

    # Push fake return address and frame pointer
    # this could be helpful to debuggers
    pushq $0
    pushq $0

    call limine_main

    # TODO: This is hacky
    # get_cpu_info()->nested_level = 0;
    movq $0, %gs:24

    jmp start_first_process


    .section .text
    .globl _cpu_entry
    .type _cpu_entry, @function
_cpu_entry:
    # Switch to stack from the limine structure
    movq 24(%rdi), %rsp

    # Switch to in-kernel page table
    movq bootstrap_cr3(%rip), %rax
    movq %rax, %cr3

    # Pass CPU struct
    movq -8(%rsp), %rdi

    # Init stack frame
    pushq $0
    pushq $0

    # Call kernel entry
    call cpu_start_routine

    movq $0, %gs:24

    jmp start_first_process

    
    .text
    .globl _acpi_wakeup_entry
    .type _acpi_wakeup_entry, @function
_acpi_wakeup_entry:
    # Switch to in-kernel stack
    leaq (stack+STACK_SIZE)(%rip), %rsp

    movq $0, %rbp

    call wakeup_main

    jmp start_first_process