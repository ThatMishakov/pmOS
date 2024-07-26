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

.equ STACK_SIZE, 0x4000

.bss
.align 4
.comm stack, STACK_SIZE

.text
.globl _limine_entry
.type _limine_entry, @function
_limine_entry:
    # If the kernel was relocatable, gp would need to be set here

    # Switch to our own stack since the one provided by limine will be unusable
    # once the kernel switched to its own page tables
    la sp, stack
    li t0, STACK_SIZE
    add sp, sp, t0

    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)
    addi fp, sp, 16

    call limine_main

    # Return to userspace
    j return_from_kernel

    .text
    .globl _cpu_bootstrap_entry
    .type _cpu_bootstrap_entry, @function
_cpu_bootstrap_entry:
    # Entry to the hart bootstrap code

    # Load the stack, since limine struct will not be available once the page table is switched
    ld sp, 32(a0)

    # Switch to the kernel page table
    la t0, satp_bootstrap_value
    ld t1, 0(t0)

    # Switch to the kernel page table
    csrw satp, t1

    # Pass CPU struct to the kernel
    ld a0, -8(sp)

    # Jump to C
    call bootstrap_entry

    # Return to userspace
    j return_from_kernel
