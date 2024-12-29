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

    .globl bochs_printc
    .type bochs_printc, @function
bochs_printc:
    movb %dil, %al
    outb $0xe9
    ret

    .globl tlb_flush
    .type tlb_flush, @function
tlb_flush:
    movq %cr3, %rax
    movq %rax, %cr3
    ret

    .globl page_clear
    .type page_clear, @function
page_clear:
    movq $512, %rcx
.start_c:
    movq $0, -8(%rdi, %rcx, 8);
    loop .start_c
    retq

    .globl loadTSS
    .type loadTSS, @function
loadTSS:
    movq %rdi, %rax
    ltr %ax
    ret

    .globl getCR3
    .type getCR3, @function
getCR3:
    movq %cr3, %rax
    ret

    .globl setCR3
    .type setCR3, @function
setCR3:
    movq %rdi, %cr3
    ret

    .globl getCR8
    .type getCR8, @function
getCR8:
    movq %cr8, %rax
    ret

    .globl setCR8
    .type setCR8, @function
setCR8:
    movq %rdi, %cr8
    ret

    .globl getCR2
    .type getCR2, @function
getCR2:
    movq %cr2, %rax
    ret

    .globl set_segment_regs
    .type set_segment_regs, @function
set_segment_regs:
    movq %rdi, %rax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    ret

    .globl read_msr
    .type read_msr, @function
read_msr:
    movl %edi, %ecx
    rdmsr
    rorq $32, %rax
    orq %rdx, %rax
    rorq $32, %rax
    ret 

    .globl write_msr
    .type write_msr, @function
write_msr:
    movl %edi, %ecx
    movl %esi, %eax
    rorq $32, %rsi
    movl %esi, %edx
    wrmsr
    ret

    .globl get_cpu_struct
    .type get_cpu_struct, @function
get_cpu_struct:
    movq %gs:0, %rax
    ret

    .globl getCR0
getCR0:
    movq %cr0, %rax
    ret

    .globl setCR0
setCR0:
    movq %rdi, %cr0
    ret

    .globl getCR4
getCR4:
    movq %cr4, %rax
    ret

    .globl setCR4
setCR4:
    movq %rdi, %cr4
    ret
