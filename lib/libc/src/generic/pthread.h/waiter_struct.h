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

#ifndef WAITER_STRUCT_H
#define WAITER_STRUCT_H

#include <pthread.h>

/// @brief Initialize a waiter struct
/// @param waiter_struct The waiter struct to initialize
/// @return 0 on success, -1 on error. Sets errno.
int __init_waiter_struct(struct __pthread_waiter *waiter_struct);

/// @brief Atomically add a waiter to the end of a list of waiters
///
/// This function adds a waiter to the list. The list is atomic and lock-free for
/// push back and pop front operations. The list is a singly linked list.
/// @param waiters_list_head Pointer to pointer to head. NULL if empty.
/// @param waiters_list_tail Pointer to pointer to tail. NULL if empty.
/// @param waiter Waiter to add to the list.
void __atomic_add_waiter(struct __pthread_waiter **waiters_list_head,
                         struct __pthread_waiter **waiters_list_tail,
                         struct __pthread_waiter *waiter);

/// @brief Try to pop the first waiter from a list
///
/// This function tries to pop the first waiter from a list. If the list is empty,
/// NULL is returned, otherwise the first node is atomically removed and returned.
/// The function is only thread-safe if no more than 1 thread calls the function
/// at any given time (but any can call add_waiter as needed).
/// @param waiters_list_head Pointer to pointer to head.
/// @param waiters_list_tail Pointer to pointer to tail.
/// @return The first waiter in the list, or NULL if the list is empty.
struct __pthread_waiter *__try_pop_waiter(struct __pthread_waiter **waiters_list_head,
                                          struct __pthread_waiter **waiters_list_tail);

#endif // WAITER_STRUCT_H
