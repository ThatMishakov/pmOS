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
    .globl syscall32_0
    .type syscall32_0, @function
syscall32_0:
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
    movl %ebx, 4(%ecx)
    movl %esi, 8(%ecx)
    movl %edi, 12(%ecx)
    movl %ecx, %eax

    popl %esi
    popl %edi
    popl %ebx

    popl %ebp
    ret

    .text
    .globl syscall32_1
    .type syscall32_1, @function
syscall32_1:
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

    .globl syscall32_2
    .type syscall32_2, @function
syscall32_2:
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
    movl 16(%ebp), %esi
    
    int $0xf8

    jmp syscall_return

    .globl syscall32_3
    .type syscall32_3, @function
syscall32_3:
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
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    
    int $0xf8

    jmp syscall_return

    .globl syscall32_4
    .type syscall32_4, @function
syscall32_4:
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
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    jmp syscall_return

    .globl syscall32_5
    .type syscall32_5, @function
syscall32_5:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $4, %esp

    jmp syscall_return

    .globl syscall32_6
    .type syscall32_6, @function
syscall32_6:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 32(%ebp)
    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $8, %esp

    jmp syscall_return

    .globl syscall32_7
    .type syscall32_7, @function
syscall32_7:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 36(%ebp)
    pushl 32(%ebp)
    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 16(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $12, %esp

    jmp syscall_return

    .globl syscall32_8
    .type syscall32_8, @function
syscall32_8:
    popl %eax
    xchgl %eax, 0(%esp)

    pushl %ebp
    movl %esp, %ebp

    pushl %ebx
    pushl %edi
    pushl %esi
    pushl %eax

    pushl 40(%ebp)
    pushl 36(%ebp)
    pushl 32(%ebp)
    pushl 28(%ebp)

    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl 26(%ebp), %esi
    movl 20(%ebp), %edi
    movl 24(%ebp), %ebp
    
    int $0xf8

    addl $16, %esp

    jmp syscall_return