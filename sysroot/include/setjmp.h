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

#ifndef __SETJMP_H
#define __SETJMP_H 1

struct __sigjmp_buf_tag {
#ifdef __riscv
    long ra;
    long sp;
    long s0;
    long s1;
    long s2;
    long s3;
    long s4;
    long s5;
    long s6;
    long s7;
    long s8;
    long s9;
    long s10;
    long s11;
#elif defined(__x86_64__)
    long rbp;
    long rbx;
    long r12;
    long r13;
    long r14;
    long r15;
    long rsp;
    long rip;
#elif defined(__i386__)
    long ebx;
    long ebp;
    long edi;
    long esi;
    long esp;
    long eip;
#else
    #error "Unsupported architecture"
#endif

    int restore_mask;
    // TODO: Signal mask, etc.
};

// This needs to be an array type, because of setjmp/longjmp signature
typedef struct __sigjmp_buf_tag sigjmp_buf[1];
typedef sigjmp_buf jmp_buf;

#ifdef __cplusplus
    #define _NORETURN [[noreturn]]
#else
    #define _NORETURN _Noreturn
#endif

#if defined(__cplusplus)
extern "C" {
#endif

int _setjmp(jmp_buf);
int setjmp(jmp_buf);
int sigsetjmp(sigjmp_buf, int);

_NORETURN void _longjmp(jmp_buf, int);
_NORETURN void longjmp(jmp_buf, int);
_NORETURN void siglongjmp(sigjmp_buf, int);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#undef _NORETURN

// #if defined(__cplusplus)

// extern "C" [[noreturn]] void longjmp(jmp_buf env, int val);

// #else
// // This cannot be a macro
// _Noreturn void longjmp(jmp_buf env, int val);
// #endif

#endif // __SETJMP_H