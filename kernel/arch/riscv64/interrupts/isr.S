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
    .type isr, @function
    .globl isr
isr:
    # Swap user (on user interrupts) thread pointer with the kernel thread pointer
    # If the interrupt is from kernel, they should be the same anyways
    csrrw tp, sscratch, tp

    # Save one temporary register and load the location where the registers
    # will be saved into it.
    # sched/sched.hh has the struct layout
    sd t0, 16(tp)

    # Tick the nested_level counter, to know if the interrupt is in kernel next time
    ld t0, 24(tp)
    bne zero, t0, kernel_interrupt

    # Entering from userspace (or kernel thread)
    addi t0, t0, 1
    sd t0, 24(tp)


    # Load the address of the task struct
    # Load task struct info
    ld t0, 32(tp)

    # Save registers
    sd x1, 8(t0)  # ra
    sd x2, 16(t0) # sp
    sd x3, 24(t0) # gp
    # tp. At the moment, it is in the sscratch register
    # t0 is in CPU struct at the moment
    sd x6, 48(t0) # t1
    sd x7, 56(t0) # t2
    sd x8, 64(t0) # s0
    sd x9, 72(t0) # s1
    sd x10, 80(t0) # a0
    sd x11, 88(t0) # a1
    sd x12, 96(t0) # a2
    sd x13, 104(t0) # a3
    sd x14, 112(t0) # a4
    sd x15, 120(t0) # a5
    sd x16, 128(t0) # a6
    sd x17, 136(t0) # a7
    sd x18, 144(t0) # s2
    sd x19, 152(t0) # s3
    sd x20, 160(t0) # s4
    sd x21, 168(t0) # s5
    sd x22, 176(t0) # s6
    sd x23, 184(t0) # s7
    sd x24, 192(t0) # s8
    sd x25, 200(t0) # s9
    sd x26, 208(t0) # s10
    sd x27, 216(t0) # s11
    sd x28, 224(t0) # t3
    sd x29, 232(t0) # t4
    sd x30, 240(t0) # t5
    sd x31, 248(t0) # t6

    # Save t0
    ld t1, 16(tp)
    sd t1, 40(t0)

    # Save tp
    # Read user tp
    csrr t1, sscratch
    # Save kernel ssratch
    csrw sscratch, tp
    # Save user tp
    sd t1, 32(t0)

    # Save program counter
    csrr t1, sepc
    sd t1, 0(t0)

    # Switch to kernel stack
    ld sp, 8(tp)

    # Set frame pointer for debugging and stuff
    addi sp, sp, -16
    sd zero, 0(sp)
    sd zero, 8(sp)
    addi fp, sp, 16


    # Jump to C++
    call handle_interrupt
    # Return from kernel



    .globl return_from_kernel
return_from_kernel:

    # nested_level counter
    ld t1, 24(tp)
    addi t1, t1, -1
    sd t1, 24(tp)

    # Load the struct holding the saved registers
    ld t0, 32(tp)

    # SPP bit
    # If the task is in user space, clear it, otherwise set it
    li t1, 0x100 # SPP bit mask
    csrc sstatus, t1
    ld t2, 264(t0)
    beq t2, zero, 1f
    # Also enable interrupts while we're here
    ori t1, t1, 0x20 # sstatus.SPIE
    csrs sstatus, t1
1:

    # Restore program counter
    ld t1, 0(t0)
    csrw sepc, t1

    # Restore registers
    ld x31, 248(t0) # t6
    ld x30, 240(t0) # t5
    ld x29, 232(t0) # t4
    ld x28, 224(t0) # t3
    ld x27, 216(t0) # s11
    ld x26, 208(t0) # s10
    ld x25, 200(t0) # s9
    ld x24, 192(t0) # s8
    ld x23, 184(t0) # s7
    ld x22, 176(t0) # s6
    ld x21, 168(t0) # s5
    ld x20, 160(t0) # s4
    ld x19, 152(t0) # s3
    ld x18, 144(t0) # s2
    ld x17, 136(t0) # a7
    ld x16, 128(t0) # a6
    ld x15, 120(t0) # a5
    ld x14, 112(t0) # a4
    ld x13, 104(t0) # a3
    ld x12, 96(t0) # a2
    ld x11, 88(t0) # a1
    ld x10, 80(t0) # a0
    ld x9, 72(t0) # s1
    ld x8, 64(t0) # s0
    ld x7, 56(t0) # t2
    ld x6, 48(t0) # t1
    ld x4, 32(t0) # tp
    ld x3, 24(t0) # gp
    ld x2, 16(t0) # sp
    ld x1, 8(t0)  # ra

    # t0 has to go last
    ld x5, 40(t0) # t0

    # Return to *wherever*
    sret


kernel_interrupt:
    addi t0, t0, 1
    sd t0, 24(tp)

    # Save context onto the stack
    andi t0, sp, 0xf
    bne t0, zero, kernel_stack_misaligned

    addi sp, sp, -256
    addi t0, sp, 256
    sd   t0, 16(sp)
    j    save_registers_common  
kernel_stack_misaligned:
    addi sp, sp, -264
    addi t0, sp, 264
    sd   t0, 16(sp)
save_registers_common:

    sd   ra, 8(sp)
    # sp
    sd   gp, 24(sp)
    # sd   tp, sp(32)
    ; sd   t0, 40(sp)
    sd   t1, 48(sp)
    sd   t2, 56(sp)
    sd   s0, 64(sp)
    sd   s1, 72(sp)
    sd   a0, 80(sp)
    sd   a1, 88(sp)
    sd   a2, 96(sp)
    sd   a3, 104(sp)
    sd   a4, 112(sp)
    sd   a5, 120(sp)
    sd   a6, 128(sp)
    sd   a7, 136(sp)
    sd   s2, 144(sp)
    sd   s3, 152(sp)
    sd   s4, 160(sp)
    sd   s5, 168(sp)
    sd   s6, 176(sp)
    sd   s7, 184(sp)
    sd   s8, 192(sp)
    sd   s9, 200(sp)
    sd   s10, 208(sp)
    sd   s11, 216(sp)
    sd   t3, 224(sp)
    sd   t4, 232(sp)
    sd   t5, 240(sp)
    sd   t6, 248(sp)

    # Save calling address
    csrr t1, sepc
    sd t1, 0(sp)

    # tp
    csrr t1, sscratch
    csrw sscratch, tp
    sd t1, 32(sp)

    ld t1, 16(tp)
    sd t1, 40(sp)

    mv fp, zero
    mv a0, sp
    call nested_interrupt

    # nested_level counter
    ld t1, 24(tp)
    addi t1, t1, -1
    sd t1, 24(tp)

    # Return from kernel
    # Restore program counter
    ld t1, 0(sp)
    csrw sepc, t1

    # Restore registers
    ld x31, 248(sp) # t6
    ld x30, 240(sp) # t5
    ld x29, 232(sp) # t4
    ld x28, 224(sp) # t3
    ld x27, 216(sp) # s11
    ld x26, 208(sp) # s10
    ld x25, 200(sp) # s9
    ld x24, 192(sp) # s8
    ld x23, 184(sp) # s7
    ld x22, 176(sp) # s6
    ld x21, 168(sp) # s5
    ld x20, 160(sp) # s4
    ld x19, 152(sp) # s3
    ld x18, 144(sp) # s2
    ld x17, 136(sp) # a7
    ld x16, 128(sp) # a6
    ld x15, 120(sp) # a5
    ld x14, 112(sp) # a4
    ld x13, 104(sp) # a3
    ld x12, 96(sp) # a2
    ld x11, 88(sp) # a1
    ld x10, 80(sp) # a0
    ld x9, 72(sp) # s1
    ld x8, 64(sp) # s0
    ld x7, 56(sp) # t2
    ld x6, 48(sp) # t1
    ld x5, 40(sp) # t0
    ld x4, 32(sp) # tp
    ld x3, 24(sp) # gp
    ld x1, 8(sp)  # ra

    # sp has to go last
    ld x2, 16(sp) # sp

    # Return to *wherever*
    sret