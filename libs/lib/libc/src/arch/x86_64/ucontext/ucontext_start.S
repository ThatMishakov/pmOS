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

    .type __ucontext_start, @function
    .globl __ucontext_start
    .hidden __ucontext_start
__ucontext_start:
    # Since the saved registers are what's restored, use them
    # to pass initial arguments to the function.
    movq %rbx, %rdi
    movq %r12, %rsi
    movq %r13, %rdx
    movq %r14, %rcx
    movq %r15, %r8
    movq %rbp, %r9

    # POSIX explicitly says that arguments have to be integers
    # so don't worry about XMM stuff

    # Restore %rbp (to 0)
    movq $0, %rbp

    # Save the uc_link pointer to preserved register
    popq %rbx

    # Call the function
    # makecontext should have left the function address on the stack
    # with the right alignment after popping it
    popq %rax
    callq *%rax

    # I've seen implementations that leave cleanup function address
    # as the return address, but it seems to leave the stack unaligned
    # (which is not up to System V ABI and leads to problems)
    # so either more assembly magic is needed or beter off just
    # call it here as normal function

    # Pass the uc_link pointer as the first argument
    movq %rbx, %rdi
    # Call the cleanup function
    call __ucontext_cleanup

    # Should never get here
    ud2