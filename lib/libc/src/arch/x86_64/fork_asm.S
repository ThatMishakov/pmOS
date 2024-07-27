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
    .globl __do_fork
    .type __do_fork, @function
__do_fork:
    pushq %rbp
    movq %rsp, %rbp

    # Align stack
    subq $8, %rsp

    # Save the caller's registers for child to restore.
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Pass the stack pointer as the third argument to C
    movq %rsp, %rdx

    # Call inner fork function
    call __libc_fork_inner

    # Don't restore registers in child.
    movq    %rbp, %rsp
    popq    %rbp

    # Return from the function
    ret

    .globl __fork_child_entry_point
__fork_child_entry_point:
    # Restore stack pointer
    movq    %rdi, %rsp

    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx

    addq    $8, %rsp

    movq    %rsp, %rbp

    # Call the child function
    call __libc_fork_child

    # Return!
    # movq    %rbp, %rsp # Not really necessary
    popq    %rbp
    ret