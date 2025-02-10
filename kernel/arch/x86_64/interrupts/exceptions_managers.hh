/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <types.hh>

// This file contains different exception managers.
// See https://wiki.osdev.org/Exceptions for what each one does.

struct NestedIntContext {
    u64 rax;
    u64 rbx;
    u64 rcx;
    u64 rdx;
    u64 rsi;
    u64 rdi;
    u64 rbp;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

constexpr int division_error_num = 0x0;
extern "C" void division_error_isr();
extern "C" void division_error_manager();

constexpr int debug_trap_num = 0x1;
extern "C" void debug_trap_isr();
extern "C" void debug_trap_manager();

constexpr int nmi_num = 0x2;
extern "C" void nmi_isr();
extern "C" void nmi_manager(NestedIntContext *kernel_ctx, ulong err);

constexpr int breakpoint_num = 0x03;
extern "C" void breakpoint_isr();
extern "C" void breakpoint_manager(NestedIntContext *kernel_ctx, ulong err);

constexpr int overflow_num = 0x4;
extern "C" void overflow_isr();
extern "C" void overflow_manager();

constexpr int invalid_opcode_num = 0x6;
extern "C" void invalid_opcode_isr();
extern "C" void invalid_opcode_manager();

constexpr int sse_exception_num = 0x7;
extern "C" void sse_exception_isr();
extern "C" void sse_exception_manager();

constexpr int double_fault_num = 0x8;
extern "C" void double_fault_isr();
extern "C" void double_fault_manager(NestedIntContext *kernel_ctx, ulong err);

constexpr int stack_segment_fault_num = 0xC;
extern "C" void stack_segment_fault_isr();
extern "C" void stack_segment_fault_manager();

constexpr int general_protection_fault_num = 0xD;
extern "C" void general_protection_fault_isr();
extern "C" void general_protection_fault_manager(NestedIntContext *kernel_ctx, ulong err);

constexpr int pagefault_num = 0xE;
extern "C" void pagefault_isr();
extern "C" void pagefault_manager(NestedIntContext *kernel_ctx, ulong err);

constexpr int simd_fp_exception_num = 0x13;
extern "C" void simd_fp_exception_isr();
extern "C" void simd_fp_exception_manager();

// Jumpto functions. This change the return %RIP address in a way that upon
// returning from the kernel exception, *function* is executed. Used for
// in-kernel exceptions to print debug information upon panic.
extern "C" void jumpto_func(void) NORETURN;
void kernel_jump_to(void (*function)(void));