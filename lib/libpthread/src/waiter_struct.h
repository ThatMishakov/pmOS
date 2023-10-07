#ifndef WAITER_STRUCT_H
#define WAITER_STRUCT_H

#include <pthread.h>

/// @brief Initialize a waiter struct
/// @param waiter_struct The waiter struct to initialize
/// @return 0 on success, -1 on error. Sets errno.
int __init_waiter_struct(struct __pthread_waiter * waiter_struct);

/// @brief Atomically add a waiter to the end of a list of waiters
///
/// This function adds a waiter to the list. The list is atomic and lock-free for
/// push back and pop front operations. The list is a singly linked list.
/// @param waiters_list_head Pointer to pointer to head. NULL if empty.
/// @param waiters_list_tail Pointer to pointer to tail. NULL if empty.
/// @param waiter Waiter to add to the list.
void __atomic_add_waiter(struct __pthread_waiter **waiters_list_head, struct __pthread_waiter **waiters_list_tail, struct __pthread_waiter* waiter);

/// @brief Try to pop the first waiter from a list
///
/// This function tries to pop the first waiter from a list. If the list is empty,
/// NULL is returned, otherwise the first node is atomically removed and returned.
/// The function is only thread-safe if no more than 1 thread calls the function
/// at any given time (but any can call add_waiter as needed).
/// @param waiters_list_head Pointer to pointer to head.
/// @param waiters_list_tail Pointer to pointer to tail.
/// @return The first waiter in the list, or NULL if the list is empty.
struct __pthread_waiter * __try_pop_waiter(struct __pthread_waiter **waiters_list_head, struct __pthread_waiter **waiters_list_tail);

#endif // WAITER_STRUCT_H
