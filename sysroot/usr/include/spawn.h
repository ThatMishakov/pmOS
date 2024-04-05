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

#ifndef _SPAWN_H
#define _SPAWN_H 1

#include <sched.h>
#include <signal.h>

#define __DECLARE_MODE_T
#define __DECLARE_PID_T
#define __DECLARE_SIGSET_T
#include "__posix_types.h"

typedef struct {
    void * params;
} posix_spawnattr_t;

typedef struct {
    void * params;
} posix_spawn_file_actions_t;

/// posix_spawnattr_setflags() constants
enum {
    POSIX_SPAWN_RESETIDS = 0; //< Reset effective user and group IDs.
    POSIX_SPAWN_SETPGROUP = 1; //< Set process group ID to that of the calling process.
    POSIX_SPAWN_SETSCHEDPARAM = 2; //< Set scheduling parameters.
    POSIX_SPAWN_SETSCHEDULER = 3; //< Set scheduling policy.
    POSIX_SPAWN_SETSIGDEF = 4; //< Set default signal actions.
    POSIX_SPAWN_SETSIGMASK = 5; //< Set signal mask.
};

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __STDC_HOSTED__

int   posix_spawn(pid_t *, const char *,
          const posix_spawn_file_actions_t *,
          const posix_spawnattr_t *, char *const [],
          char *const []);
          
int   posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *,
          int);
int   posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *,
          int, int);
int   posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *,
          int, const char *, int, mode_t);
int   posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *);
int   posix_spawn_file_actions_init(posix_spawn_file_actions_t *);
int   posix_spawnattr_destroy(posix_spawnattr_t *);
int   posix_spawnattr_getflags(const posix_spawnattr_t *,
          short *);
int   posix_spawnattr_getpgroup(const posix_spawnattr_t *,
          pid_t *);
          int   posix_spawnattr_getschedparam(const posix_spawnattr_t *,
          struct sched_param *);
int   posix_spawnattr_getschedpolicy(const posix_spawnattr_t *,
          int *);
int   posix_spawnattr_getsigdefault(const posix_spawnattr_t *,
          sigset_t *);
int   posix_spawnattr_getsigmask(const posix_spawnattr_t *,
          sigset_t *);
int   posix_spawnattr_init(posix_spawnattr_t *);
int   posix_spawnattr_setflags(posix_spawnattr_t *, short);
int   posix_spawnattr_setpgroup(posix_spawnattr_t *, pid_t);
int   posix_spawnattr_setschedparam(posix_spawnattr_t *,
          const struct sched_param *);
int   posix_spawnattr_setschedpolicy(posix_spawnattr_t *, int);
int   posix_spawnattr_setsigdefault(posix_spawnattr_t *,
          const sigset_t *);
int   posix_spawnattr_setsigmask(posix_spawnattr_t *,
          const sigset_t *);
int   posix_spawnp(pid_t *, const char *,
          const posix_spawn_file_actions_t *,
          const posix_spawnattr_t *,
          char *const [], char *const []);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SPAWN_H