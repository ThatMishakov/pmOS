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

#include <errno.h>
#include <pmos/tls.h>
#include <stddef.h>
#include <stdlib.h>

struct uthread *__get_tls();

void __call_thread_atexit()
{
    struct uthread *u = __get_tls();
    if (u == NULL)
        return;

    struct __atexit_list_entry *entry = u->atexit_list_head, *next;
    while (entry != NULL) {
        entry->destructor_func(entry->arg);
        next = entry->next;
        free(entry);
        entry = next;
    }
}

int __cxa_thread_atexit_impl(void (*func)(void *), void *arg, void *dso_handle)
{
    // TODO: This will need to be revised when we have dynamic linking
    struct uthread *u = __get_tls();
    if (u == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct __atexit_list_entry *entry = malloc(sizeof(struct __atexit_list_entry));
    if (entry == NULL) {
        // malloc() sets errno
        // errno = ENOMEM;
        return -1;
    }

    *entry = (struct __atexit_list_entry) {
        .destructor_func = func,
        .arg             = arg,
        .dso_handle      = dso_handle,
        .next            = u->atexit_list_head,
    };

    u->atexit_list_head = entry;

    return 0;
}