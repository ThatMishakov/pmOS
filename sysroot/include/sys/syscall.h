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

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

/**
 * @brief Generic system call function.
 *
 * This is a generic system call function, present for compatibility with
 * other UNIX-like operating systems and libraries that expect it. It is
 * not used by the standard library. Since the System-V ABI specifies
 * two return registers, the kernel typically returns two 64-bit values,
 * containing the result (or error code) in the first one, and simple
 * return values (such as sizes, pointers, IDs, etc.) in the second one. This function
 * ignores the second value, and only returns the result code. If the result is
 * wanted (and for better compatibility in general), the native pmos_syscall()
 * function should be used instead.
 *
 * @param number System call number
 * @param ... System call arguments. The number and types of arguments
 *            depend on the system call, and could be up to 6 64-bit values.
 * @return int Result of the system call. On success, 0 is returned. On error, -1 is
 *             returned and the actual kernel return code is translated to the appropriate
 *             POSIX error and is stored in errno.
 * @see pmos_syscall()
 */
int syscall(int number, ...);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SYS_SYSCALL_H