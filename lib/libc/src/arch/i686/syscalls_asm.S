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

#     .text
#     .globl pmos_syscall
#     .type pmos_syscall, @function
# pmos_syscall:
#     # ----- Software int -------

#     xchgw %bx, %bx
#     int $0xf8
#     xchgw %bx, %bx


#     # ----- syscall -------
#     # movq %rcx, %r10
#     # syscall

#     # ------ sysenter -------
#     # movq %rcx, %r10
#     # movq %rdx, %r11
#     # leaq sysenter_ret(%rip), %rdx
#     # movq %rsp, %rcx

#     # sysenter
# # sysenter_ret:
#     # movq %r11, %rdx

#     ret

    .text
    .globl __pmos_syscall32_0words
    .type __pmos_syscall32_0words, @function
__pmos_syscall32_0words:
    popl %eax
    xchgl %eax, 0(%esp)
    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax

    int $0xf8

syscall_return:
    popl %ecx
    movl %eax, (%ecx)
    movl %edx, 4(%ecx)
    movl %esi, 8(%ecx)
    movl %edi, 12(%ecx)
    movl %ecx, %eax

    popl %esi
    popl %edi
    popl %ebx

    popl %ebp
    ret

    .text
    .globl __pmos_syscall32_1words
    .type __pmos_syscall32_1words, @function
__pmos_syscall32_1words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    
    int $0xf8

    jmp syscall_return

    .globl __pmos_syscall32_2words
    .type __pmos_syscall32_2words, @function
__pmos_syscall32_2words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %ecx
    
    int $0xf8

    jmp syscall_return

    .globl __pmos_syscall32_3words
    .type __pmos_syscall32_3words, @function
__pmos_syscall32_3words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edx
    
    int $0xf8

    jmp syscall_return

    .globl __pmos_syscall32_4words
    .type __pmos_syscall32_4words, @function
__pmos_syscall32_4words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edx
    movl 24(%ebp), %ebp
    
    int $0xf8

    jmp syscall_return

    .globl __pmos_syscall32_5words
    .type __pmos_syscall32_5words, @function
__pmos_syscall32_5words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 28(%ebp), %eax
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edx
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $4, %esp

    jmp syscall_return

    .globl __pmos_syscall32_6words
    .type __pmos_syscall32_6words, @function
__pmos_syscall32_6words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 32(%ebp), %eax
    pushl %eax
    movl 28(%ebp), %eax
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edx
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $8, %esp

    jmp syscall_return

    .globl __pmos_syscall32_7words
    .type __pmos_syscall32_7words, @function
__pmos_syscall32_7words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 36(%ebp), %eax
    pushl %eax
    movl 32(%ebp), %eax
    pushl %eax
    movl 28(%ebp), %eax
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %ecx
    movl 20(%ebp), %edx
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $12, %esp

    jmp syscall_return

    .globl __pmos_syscall32_8words
    .type __pmos_syscall32_8words, @function
__pmos_syscall32_8words:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    movl 40(%ebp), %eax
    pushl %eax
    movl 36(%ebp), %eax
    pushl %eax
    movl 32(%ebp), %eax
    pushl %eax
    movl 28(%ebp), %eax
    pushl %eax

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 26(%ebp), %ecx
    movl 20(%ebp), %edx
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $16, %esp

    jmp syscall_return