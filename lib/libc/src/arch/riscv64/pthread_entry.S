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
    .type __get_gp, @function
    .globl __get_gp
    .hidden __get_gp
__get_gp:
    mv a0, gp
    ret

    .type __get_tp, @function
    .globl __get_tp
    .hidden __get_tp
__get_tp:
    mv a0, tp
    ret    

    .type __pthread_entry_point, @function
    .globl __pthread_entry_point
    # C function declaration:
    # void __pthread_entry_point(void *(*start_routine) (void *),
    #                            void * arg) __attribute__((noreturn));
__pthread_entry_point:
    # Set up stack frame
    mv ra, zero
    mv fp, zero

    # Function prelude
    addi sp, sp, -16
    sd fp, 0(sp)
    sd ra, 8(sp)
    addi fp, sp, 16

    # This is kinda bad but it's ok for now, everyone can pretend it's not here...
    # (this gets set in the crt0)
    .option push
	.option norelax
	la t0, __libc_gp
    ld gp, 0(t0)
	.option pop

    # Move the arguments in the right registers
    # Start routine
    mv t0, a0
    # Second argument of this function -> first argument of start_routine
    mv a0, a1

    # Call the start_routine
    jalr ra, t0, 0

    # Exit the thread
    # Return value is also the first argument
    call __pthread_exit

    # Should never reach here
    unimp




    .type __pthread_exit, @function
    .globl __pthread_exit
__pthread_exit:
    # The stack has to be aligned to 16 bytes
    addi sp, sp, -48

    # Do the prelude for debugging and stuff
    sd fp, 32(sp)
    sd ra, 40(sp)
    addi fp, sp, 48

    # Save the saved registers
    # Doesn't really matter as the stack will be freed anyway
    # but can be useful for debugging
    sd s1, 0(sp)
    sd s2, 8(sp)
    sd s3, 16(sp)

    # Save the return value, so it can survive untill syscall_exit
    mv s1, a0

    # Get thread local data
    call __get_tls
    mv s3, a0

    # Save data to TLS and stuff; see x86_64 and sysroot/usr/include/pmos/tls.h
    # for more info
    sd s1, 0x18(s3) # void * return_value
    # Load stack pointer into saved registers, to be accessible after
    # destroying TLS
    ld s2, 0x08(s3) # void * stack_top
    ld s3, 0x10(s3) # void * stack_size

    # Take care of destructors
    # This should also destroy the TLS
    call __thread_exit_fire_destructors

    # Free the stack!
    # The stack is allocated as a memory region
    # Syscalls have to be called mamually here since there's no stack
    # for C functions...

    # Syscall ID
    li a0, 26 # SYSCALL_UNMAP_RANGE
    # uint64_t pid (TID in reality, but I digress)
    mv a1, zero # PID_SELF
    # void * region_start
    sub a2, s2, s3 # a2 = stack_top - stack_size
    # size_t region_size
    mv a3, s3
    # Zero out the rest of registers
    mv a3, zero
    mv a4, zero
    mv a5, zero
    mv a6, zero

    # Demolish the stack!
    ecall


    # Exit the task...
    # Syscall ID
    li a0, 0 # SYSCALL_EXIT
    # Return value
    mv a1, s1
    ecall

    # Crash if the syscall fails (which it shouldn't)
    unimp

