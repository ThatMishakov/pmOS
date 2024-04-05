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

#include <stdlib.h>
#include <pthread.h>

// Pass the pointer for __cxa_atexit
// Normal atexit() functions should just ignore it
typedef void (*atexit_func_t)(void *);

/// Linked list of functions to call on exit
struct atexit_func_entry_t {
    struct atexit_func_entry_t * prev;
    struct atexit_func_entry_t * next;

    /// @brief Function to call on exit
    atexit_func_t destructor_func;

    /// @brief Argument to pass to the function. Only used for C++ objects.
    void *arg;

    /// @brief DSO handle where the function was registered. If it gets destroyed, the functions are called.
    void *dso_handle;
};

static struct atexit_func_entry_t * atexit_func_head = NULL;
static struct atexit_func_entry_t * atexit_func_tail = NULL;

// I don't anticipate atexit-related functions to be called a lot, so a spinlock should be fine
pthread_spinlock_t __atexit_funcs_lock = 0;

int __cxa_atexit(void (*func) (void *), void * arg, void * dso_handle)
{
    struct atexit_func_entry_t * new_func = malloc(sizeof(struct atexit_func_entry_t));
    if (!new_func) {
        // errno is set by malloc
        return -1;
    }

    new_func->destructor_func = func;
    new_func->arg = arg;
    new_func->dso_handle = dso_handle;
    new_func->next = NULL;

    pthread_spin_lock(&__atexit_funcs_lock);

    if (atexit_func_tail)
        atexit_func_tail->next = new_func;
    else
        atexit_func_head = new_func;
    
    new_func->prev = atexit_func_tail;
    atexit_func_tail = new_func;

    pthread_spin_unlock(&__atexit_funcs_lock);

    return 0;
}

int atexit(void (*func)(void))
{
    return __cxa_atexit((void (*)(void *))func, NULL, NULL);
}

void _atexit_pop_all()
{
    while (atexit_func_tail) {
        struct atexit_func_entry_t * func = atexit_func_tail;
        atexit_func_tail = func->prev;

        func->destructor_func(func->arg);
        free(func);
    }
    atexit_func_head = NULL;
}
