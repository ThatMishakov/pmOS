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

#ifndef __SYS_WAIT_H
#define __SYS_WAIT_H
#include "../signal.h"
#include "resource.h"

/// Constants for the `waitpid' function.
enum {
    WCONTINUED = 1, //< Report status of continued child process.
    WNOHANG = 2, //< Return immediately if no child has exited.
    WUNTRACED = 4, //< Report status of stopped child process.
};

/// Exit status of a child process.
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
/// Returns true if the child process was continued.
#define WIFCONTINUED(status) ((status) == 0xffff)
/// Returns true if the child process exited normally.
#define WIFEXITED(status) (((status) & 0xff) == 0)
/// Returns true if the child process was terminated by a signal.
#define WIFSIGNALED(status) (((status) & 0xff) != 0 && ((status) & 0xff00) == 0)
/// Returns true if the child process was stopped.
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
/// Returns the signal that stopped the child process.
#define WSTOPSIG(status) (((status) & 0xff00) >> 8)
/// Returns the signal that terminated the child process.
#define WTERMSIG(status) ((status) & 0xff)

/// Constants for the `waitid' function.
enum {
    WEXITED = 0, //< Wait for processes that have exited.
    WNOWAIT = 1, //< Leave process running; just report status.
    WSTOPPED = 2, //< Wait for processes that have stopped.
};

#define __DECLARE_IDTYPE_T
#define __DECLARE_ID_T
#define __DECLARE_PID_T
#include "../__posix_types.h"

#if defined(__cplusplus)
extern "C" {
#endif
#ifdef __STDC_HOSTED__

pid_t  wait(int *);
int    waitid(idtype_t, id_t, siginfo_t *, int);
pid_t  waitpid(pid_t, int *, int);


#endif /* __STDC_HOSTED__ */

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif