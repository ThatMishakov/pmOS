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
    pushl %ebp
    movl %esp, %ebp

    # Save the caller's registers for child to restore.
    pushl %ebx
    pushl %esi
    pushl %edi

    # Align the stack to 16 bytes
    subl $8, %esp
    
    # Pass the stack pointer as the third argument to C
    pushl %esp

    # Pass 2 arguments to C
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    pushl %ebx
    pushl %eax

    # Call inner fork function
    call __libc_fork_inner

    # Don't restore registers in child.
    movl %ebp, %esp
    popl %ebp

    # Return from the function
    ret

    .globl __fork_child_entry_point
__fork_child_entry_point:
    # Restore stack pointer
    movl %eax, %esp
    addl $8, %esp

    popl %edi
    popl %esi
    popl %ebx

    movl %esp, %ebp
    addl $8, %esp

    # Call the child function
    call __libc_fork_child

    # Return!
    # movl %ebp, %esp # Not really necessary
    popl %ebp
    ret