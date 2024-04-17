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
    .globl acquire_lock_spin
    .type acquire_lock_spin, @function
acquire_lock_spin:
    addi sp, sp, -16
    sd fp, 0(sp)
    sd ra, 8(sp)
    addi fp, sp, 16

    li t0, 1
loop:
    lw t1, (a0)
    bnez t1, loop
    amoswap.w.aq t1, t0, (a0)
    bnez t1, loop

    ld fp, 0(sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    
    ret

    .globl release_lock
    .type release_lock, @function
release_lock:
    amoswap.w.rl zero, zero, (a0)
    ret

    .globl try_lock
    .type try_lock, @function
try_lock:
    li t0, 1
    lw t1, (a0)
    beqz t1, fail
    amoswap.w.aq t1, t0, (a0)
    beqz t1, fail
    li a0, 1
    ret
fail:
    li a0, 0
    ret