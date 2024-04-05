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
    .globl fp_write_regs_initial
    .type fp_write_regs_initial, @function
fp_write_regs_initial:
    addi sp, sp, -16
    sd zero, 0(sp)
    fsw f0, 0(sp)
    fsw f1, 0(sp)
    fsw f2, 0(sp)
    fsw f3, 0(sp)
    fsw f4, 0(sp)
    fsw f5, 0(sp)
    fsw f6, 0(sp)
    fsw f7, 0(sp)
    fsw f8, 0(sp)
    fsw f9, 0(sp)
    fsw f10, 0(sp)
    fsw f11, 0(sp)
    fsw f12, 0(sp)
    fsw f13, 0(sp)
    fsw f14, 0(sp)
    fsw f15, 0(sp)
    fsw f16, 0(sp)
    fsw f17, 0(sp)
    fsw f18, 0(sp)
    fsw f19, 0(sp)
    fsw f20, 0(sp)
    fsw f21, 0(sp)
    fsw f22, 0(sp)
    fsw f23, 0(sp)
    fsw f24, 0(sp)
    fsw f25, 0(sp)
    fsw f26, 0(sp)
    fsw f27, 0(sp)
    fsw f28, 0(sp)
    fsw f29, 0(sp)
    fsw f30, 0(sp)
    fsw f31, 0(sp)
    addi sp, sp, 16
    ret

    .globl fp_store_regs_single
    .type fp_store_regs_single, @function
fp_store_regs_single:
    fsw f0, 0(a0)
    fsw f1, 4(a0)
    fsw f2, 8(a0)
    fsw f3, 12(a0)
    fsw f4, 16(a0)
    fsw f5, 20(a0)
    fsw f6, 24(a0)
    fsw f7, 28(a0)
    fsw f8, 32(a0)
    fsw f9, 36(a0)
    fsw f10, 40(a0)
    fsw f11, 44(a0)
    fsw f12, 48(a0)
    fsw f13, 52(a0)
    fsw f14, 56(a0)
    fsw f15, 60(a0)
    fsw f16, 64(a0)
    fsw f17, 68(a0)
    fsw f18, 72(a0)
    fsw f19, 76(a0)
    fsw f20, 80(a0)
    fsw f21, 84(a0)
    fsw f22, 88(a0)
    fsw f23, 92(a0)
    fsw f24, 96(a0)
    fsw f25, 100(a0)
    fsw f26, 104(a0)
    fsw f27, 108(a0)
    fsw f28, 112(a0)
    fsw f29, 116(a0)
    fsw f30, 120(a0)
    fsw f31, 124(a0)
    ret

    .globl fp_store_regs_double
    .type fp_store_regs_double, @function
fp_store_regs_double:
    fsd f0, 0(a0)
    fsd f1, 8(a0)
    fsd f2, 16(a0)
    fsd f3, 24(a0)
    fsd f4, 32(a0)
    fsd f5, 40(a0)
    fsd f6, 48(a0)
    fsd f7, 56(a0)
    fsd f8, 64(a0)
    fsd f9, 72(a0)
    fsd f10, 80(a0)
    fsd f11, 88(a0)
    fsd f12, 96(a0)
    fsd f13, 104(a0)
    fsd f14, 112(a0)
    fsd f15, 120(a0)
    fsd f16, 128(a0)
    fsd f17, 136(a0)
    fsd f18, 144(a0)
    fsd f19, 152(a0)
    fsd f20, 160(a0)
    fsd f21, 168(a0)
    fsd f22, 176(a0)
    fsd f23, 184(a0)
    fsd f24, 192(a0)
    fsd f25, 200(a0)
    fsd f26, 208(a0)
    fsd f27, 216(a0)
    fsd f28, 224(a0)
    fsd f29, 232(a0)
    fsd f30, 240(a0)
    fsd f31, 248(a0)
    ret

    .globl fp_load_regs_single
    .type fp_load_regs_single, @function
fp_load_regs_single:
    flw f0, 0(a0)
    flw f1, 4(a0)
    flw f2, 8(a0)
    flw f3, 12(a0)
    flw f4, 16(a0)
    flw f5, 20(a0)
    flw f6, 24(a0)
    flw f7, 28(a0)
    flw f8, 32(a0)
    flw f9, 36(a0)
    flw f10, 40(a0)
    flw f11, 44(a0)
    flw f12, 48(a0)
    flw f13, 52(a0)
    flw f14, 56(a0)
    flw f15, 60(a0)
    flw f16, 64(a0)
    flw f17, 68(a0)
    flw f18, 72(a0)
    flw f19, 76(a0)
    flw f20, 80(a0)
    flw f21, 84(a0)
    flw f22, 88(a0)
    flw f23, 92(a0)
    flw f24, 96(a0)
    flw f25, 100(a0)
    flw f26, 104(a0)
    flw f27, 108(a0)
    flw f28, 112(a0)
    flw f29, 116(a0)
    flw f30, 120(a0)
    flw f31, 124(a0)
    ret

    .globl fp_load_regs_double
    .type fp_load_regs_double, @function
fp_load_regs_double:
    fld f0, 0(a0)
    fld f1, 8(a0)
    fld f2, 16(a0)
    fld f3, 24(a0)
    fld f4, 32(a0)
    fld f5, 40(a0)
    fld f6, 48(a0)
    fld f7, 56(a0)
    fld f8, 64(a0)
    fld f9, 72(a0)
    fld f10, 80(a0)
    fld f11, 88(a0)
    fld f12, 96(a0)
    fld f13, 104(a0)
    fld f14, 112(a0)
    fld f15, 120(a0)
    fld f16, 128(a0)
    fld f17, 136(a0)
    fld f18, 144(a0)
    fld f19, 152(a0)
    fld f20, 160(a0)
    fld f21, 168(a0)
    fld f22, 176(a0)
    fld f23, 184(a0)
    fld f24, 192(a0)
    fld f25, 200(a0)
    fld f26, 208(a0)
    fld f27, 216(a0)
    fld f28, 224(a0)
    fld f29, 232(a0)
    fld f30, 240(a0)
    fld f31, 248(a0)
    ret