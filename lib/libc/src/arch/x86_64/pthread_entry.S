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

    .section .text
    .type __pthread_entry_point, @function
    .globl __pthread_entry_point
__pthread_entry_point:
    # Set up stack frame

    movq $0, %rbp
    pushq %rbp
	pushq %rbp

    
    # Move the first argument (the function pointer) into %rax
    movq %rdi, %rax
    # Move the second argument (the argument pointer) into %rdi (first function argument)
    movq %rsi, %rdi

    # Call the function
    callq *%rax


    # Pass the return value of the function to pthread_exit()
    movq %rax, %rdi
    # pthread_exit() outa here!
    # Although it's noreturn, it's still need to be invoked with call and not jmp to maintain stack alignment
    call __pthread_exit




    .type __pthread_exit, @function
    .globl __pthread_exit
__pthread_exit:
    # This function calls appropriate destructors (__cxa_thread_atexit stuff and also atexit() stuff if it's the last thread),
    # destroys the TLS (if the thread is not detached), frees its stack (since kernel doesn't manage it) and exits the task

    # Do the normal stack frame stuff to be compatible with the C ABI and maintain stack alignment
    pushq %rbp
    movq %rsp, %rbp

    # Save saved registers (doesn't matter since this function exits)
    # pushq %r15
    # pushq %r14
    # pushq %r13

    # The TLS struct with explanations is declared in sysroot/usr/include/pmos/tls.h
    # Save the return value to the TLS
    movq %rax, %fs:0x18 # void * return_value
    movq %rax, %r15 # return_value

    # Save the stack data into a protected registers (so that it can be used after destroying TLS)
    movq %fs:0x8, %r14 # void * stack_top
    movq %fs:0x10, %r13 # size_t stack_size

    # Call __cxa_thread_atexit destructors and stuff
    # This function is defined somewhere in libc
    # New! This also destroys TLS if the thread is not detached
    callq __thread_exit_fire_destructors
    

    # Free the stack!
    # Since the is no stack, syscalls have to be done manually from here on

    # This is calling release_region() syscall, declared in <pmos/memory.h>
    # TODO: %rax is a convention but it's not used in pmOS
    # Syscall ID
    movq $25, %rdi # SYSCALL_DELETE_REGION
    # uint64_t pid
    movq $0, %rsi # PID_SELF
    # void * region
    movq %r14, %rdx # stack_top
    subq %r13, %rdx # stack_top - stack_size = stack_bottom -> start of the memory region
    # The rest of registers are not used. Zero them out for future compatibility and whatnot
    xorq %r10, %r10 # %r10 is used instead of %rcx because syscall instruction owerwrites it
    xorq %r8, %r8
    xorq %r9, %r9

    # zap the stack!
    syscall
    # It doesn't really matter if it succeeds or not, since the thread is exiting anyway


    # Exit the task
    movq $0, %rdi # SYSCALL_EXIT
    movq %r15, %rsi # return_value
    syscall
    # The task is dead, long live the task!

    # Crash the task if the exit syscall fails
    hlt