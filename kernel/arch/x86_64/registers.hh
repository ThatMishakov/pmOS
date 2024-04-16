/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

/// @brief %RFLAGS register bits
struct PACKED RFLAGS_Bits {
    u8 carry_flag  :1;
    u8 reserved0   :1;
    u8 parity_flag :1;
    u8 reserved1   :1;
    u8 aux_carry   :1;
    u8 reserved2   :1;
    u8 zero        :1;
    u8 sign        :1;
    u8 trap        :1;
    u8 interrupt_e :1;
    u8 direction   :1;
    u8 overflow    :1;
    u8 iopl        :2;
    u8 nested_task :1;
    u8 reserved3   :1;
    u8 resume      :1;
    u8 virtual_m   :1;
    u8 allignment  :1;
    u8 virtual_i_f :1;
    u8 virtual_i_p :1;
    u8 id          :1;
    u64 reserved4  :42;
};

union RFLAGS {
    u64 numb = 0b1000000000;
    RFLAGS_Bits bits;
};

struct Interrupt_Stackframe {
    u64 rip = 0;
    u64 cs = 0;
    RFLAGS rflags;
    u64 rsp = 0;
    u64 ss = 0;
};

/// @brief Scratch registers of the process.
struct Scratch_Regs {
    u64 rdi = 0;
    u64 rsi = 0;
    u64 rdx = 0;
    u64 rcx = 0;
    u64 r8 = 0;
    u64 r9 = 0;
    u64 rax = 0;
    u64 r10 = 0;
    u64 r11 = 0;
};

/// Preserved registers of the process. By the conventions, kernel should not change those
/// upon switching to kernel unless the process is aware about it.
struct Preserved_Regs {
    u64 rbx = 0;
    u64 rbp = 0;
    u64 r12 = 0;
    u64 r13 = 0;
    u64 r14 = 0;
    u64 r15 = 0;
};

/// Segment registers. Even though the segmentation is dead, on x86_64 these are still usefull and %gs is used for storing the thread-local data offset.
struct Segment_Offsets {
    u64 fs = 0;
    u64 gs = 0;
};

#define ENTRY_INTERRUPT 0
#define ENTRY_SYSCALL   1
#define ENTRY_SYSENTER  2
#define ENTRY_NESTED    3

/// Registers of the process. These are saved upon entering the kernel and restored upon returning.
struct X86_64Regs { // 208 bytes
    Scratch_Regs scratch_r; //72 bytes
    Interrupt_Stackframe e; // 40 bytes
    Preserved_Regs preserved_r; // 48 bytes
    Segment_Offsets seg; // 16 bytes
    u64 entry_type = ENTRY_INTERRUPT; // 8 bytes
    u64 int_err = 0; // 8
    u64 error_instr = 0; // 8
    u64 saved_entry_type = 0; // 8

        // Program counter
    inline u64 &program_counter() { return e.rip; }
    inline u64 program_counter() const { return e.rip; };
    // Stack pointer
    inline u64 &stack_pointer() { return e.rsp; }
    inline u64 stack_pointer() const { return e.rsp; };

    inline u64 &thread_pointer() { return seg.fs; }
    inline u64 thread_pointer() const { return seg.fs; };
    inline u64 &global_pointer() { return seg.gs; }
    inline u64 global_pointer() const { return seg.gs; };

    // Register holding syscall number
    // TODO: Other operating systems usually use a different register
    inline u64& syscall_number() { return scratch_r.rdi; }
    inline u64 syscall_number() const { return scratch_r.rdi; }

    // Get syscall arguments, starting from 1
    // Although SYSCALL/SYSRET use %rcx to save program pointer, the handler saves the argument to scratch_r.rcx
    // as if the kernel was entered by interrupt
    inline u64& syscall_arg1() { return scratch_r.rsi; }
    inline u64& syscall_arg2() { return scratch_r.rdx; }
    inline u64& syscall_arg3() { return scratch_r.rcx; }
    inline u64& syscall_arg4() { return scratch_r.r8; }
    inline u64& syscall_arg5() { return scratch_r.r9; }

    // Get syscall return value registers
    inline u64& syscall_retval_low()  { return scratch_r.rax; }
    inline u64& syscall_retval_high() { return scratch_r.rdx; }

    // Registers holding function arguments
    inline u64& arg1() { return scratch_r.rdi; }
    inline u64& arg2() { return scratch_r.rsi; }
    inline u64& arg3() { return scratch_r.rdx; }
    inline u64& arg4() { return scratch_r.rcx; }
    inline u64& arg5() { return scratch_r.r8; }
    inline u64& arg6() { return scratch_r.r9; }

    inline void request_syscall_restart()
    {
        saved_entry_type = entry_type;
        entry_type = ENTRY_NESTED;
    }
    inline void clear_syscall_restart()
    {
        entry_type = saved_entry_type;
        saved_entry_type = 0;
    }
};

using Task_Regs = X86_64Regs;