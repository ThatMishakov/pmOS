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
    .globl _start
_start:
    # Set up frame pointer
    # RISC-V Calling convention says it's optional, but it doesn't
    # hurt to include it
    #
    # This can be used to make GCC generate frame pointers:
    # riscv64-linux-gnu-g++ -O1 -fno-omit-frame-pointer -S test.cc
    addi sp, sp, -16
    # Since this is the first frame, set everything to 0
    sd zero, 0(sp) # Previous frame pointer
    sd zero, 8(sp) # Return address
    # Unlike x86, this points to the top of the stack frame
    addi fp, sp, 16
    # Also, stack needs to be 16-byte aligned, but it already is

    # Load global pointer
    # https://five-embeddev.com/quickref/global_pointer.html
    .option push
	.option norelax
	la gp, __global_pointer$
	.option pop

    # This is casually hacky...
    # (and will casually break with dynamic linking)
    .option push
	.option norelax
    la t0, __libc_gp
    sd gp, 0(t0)
    .option pop

    # Initialize libc
    call init_std_lib

    # Call main function of the worker
    # This is the first function that runs, and it spawns a thread for the actual main
    call worker_main

    # This should never be reached
    # UNIMP should always cause an illegal instruction exception
    unimp

    .section .text
    .type __main, @function
    .global __main
    # Linker order weirdness/hack
    # This needs to be fixed, but for now, this works
__main:
    j main