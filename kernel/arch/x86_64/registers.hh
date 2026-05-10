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

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1
#define ENTRY_SYSENTER  2
#define ENTRY_NESTED    3

/// Registers of the process. These are saved upon entering the kernel and
/// restored upon returning.
struct X86_64Regs {                         // 208 bytes
    u64 rdi = 0;    // Scratch regs (72 bytes)
    u64 rsi = 0;
    u64 rdx = 0;
    u64 rcx = 0;
    u64 r8  = 0;
    u64 r9  = 0;
    u64 rax = 0;
    u64 r10 = 0;
    u64 r11 = 0;

    u64 rip = 0; // Interrupt frame (40 bytes)
    u64 cs  = 0;
    u64 rflags = 0b1000000000;
    u64 rsp = 0;
    u64 ss  = 0;
    
    u64 rbx = 0; // Preserved regs (48 bytes), from 112
    u64 rbp = 0;
    u64 r12 = 0;
    u64 r13 = 0;
    u64 r14 = 0;
    u64 r15 = 0;

    u64 fs = 0; // 16 bytes, from 160
    u64 gs = 0;  

    u64 entry_type       = ENTRY_INTERRUPT; // 8 bytes
    u64 int_err          = 0;               // 8
    u64 error_instr      = 0;               // 8
    u64 saved_entry_type = 0;               // 8

    // Program counter
    inline u64 &program_counter() { return rip; }
    inline u64 program_counter() const { return rip; };
    // Stack pointer
    inline u64 &stack_pointer() { return rsp; }
    inline u64 stack_pointer() const { return rsp; };

    inline u64 &thread_pointer() { return fs; }
    inline u64 thread_pointer() const { return fs; };
    inline u64 &global_pointer() { return gs; }
    inline u64 global_pointer() const { return gs; };

    // Register holding syscall number
    // TODO: Other operating systems usually use a different register
    inline u64 &syscall_number() { return rdi; }
    inline u64 syscall_number() const { return rdi; }
    u64 syscall_flags() const;

    // Get syscall arguments, starting from 1
    // Although SYSCALL/SYSRET use %rcx to save program pointer, the handler
    // saves the argument to scratch_r.rcx as if the kernel was entered by
    // interrupt
    inline u64 &syscall_arg1() { return rsi; }
    inline u64 &syscall_arg2() { return rdx; }
    inline u64 &syscall_arg3() { return rcx; }
    inline u64 &syscall_arg4() { return r8; }
    inline u64 &syscall_arg5() { return r9; }

    // Get syscall return value registers
    inline u64 &syscall_retval_low() { return rax; }
    inline u64 &syscall_retval_high() { return rdx; }

    // Registers holding function arguments
    inline u64 &arg1() { return rdi; }
    inline u64 &arg2() { return rsi; }
    inline u64 &arg3() { return rdx; }
    inline u64 &arg4() { return rcx; }
    inline u64 &arg5() { return r8; }
    inline u64 &arg6() { return r9; }

    inline void request_syscall_restart()
    {
        saved_entry_type = entry_type;
        entry_type       = ENTRY_NESTED;
    }
    inline void clear_syscall_restart()
    {
        entry_type       = saved_entry_type;
        saved_entry_type = 0;
    }

    inline bool syscall_pending_restart() const { return entry_type == ENTRY_NESTED; }

    inline unsigned long xbp() const { return rbp; }
    inline unsigned long get_cs() const { return cs; }
};

using Task_Regs = X86_64Regs;