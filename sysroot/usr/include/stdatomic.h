#ifndef _STDATOMIC_H
#define _STDATOMIC_H

#include <stdbool.h>

// Define atomic types based on GCC builtins
typedef _Bool atomic_bool;
typedef char atomic_char;
typedef unsigned char atomic_uchar;
typedef short atomic_short;
typedef unsigned short atomic_ushort;
typedef int atomic_int;
typedef unsigned int atomic_uint;
typedef long atomic_long;
typedef unsigned long atomic_ulong;
typedef long long atomic_llong;
typedef unsigned long long atomic_ullong;
typedef void* atomic_ptrdiff_t;
typedef unsigned atomic_size_t;
typedef long long atomic_int_least64_t;
typedef unsigned long long atomic_uint_least64_t;
typedef int atomic_int_fast32_t;
typedef unsigned int atomic_uint_fast32_t;

// Define atomic_flag as a plain int
typedef int atomic_flag;

#define ATOMIC_FLAG_INIT 0

#define ATOMIC_VAR_INIT(value) (value)

typedef enum {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#define ATOMIC_BOOL_LOCK_FREE __ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE __ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE __ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE __ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE __ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE __ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE __ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE __ATOMIC_POINTER_LOCK_FREE

#define atomic_is_lock_free(obj) __atomic_is_lock_free(sizeof(*(obj)), (obj))

#define atomic_store(obj, desired) __atomic_store_n((obj), (desired), __ATOMIC_SEQ_CST)
#define atomic_load(obj) __atomic_load_n((obj), __ATOMIC_SEQ_CST)
#define atomic_exchange(obj, desired) __atomic_exchange_n((obj), (desired), __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_strong(obj, expected, desired) \
    __atomic_compare_exchange_n((obj), (expected), (desired), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_weak(obj, expected, desired) \
    __atomic_compare_exchange_n((obj), (expected), (desired), true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define atomic_fetch_add(obj, operand) __atomic_fetch_add((obj), (operand), __ATOMIC_SEQ_CST)
#define atomic_fetch_sub(obj, operand) __atomic_fetch_sub((obj), (operand), __ATOMIC_SEQ_CST)
#define atomic_fetch_and(obj, operand) __atomic_fetch_and((obj), (operand), __ATOMIC_SEQ_CST)
#define atomic_fetch_or(obj, operand) __atomic_fetch_or((obj), (operand), __ATOMIC_SEQ_CST)
#define atomic_fetch_xor(obj, operand) __atomic_fetch_xor((obj), (operand), __ATOMIC_SEQ_CST)

#define atomic_flag_test_and_set(obj) __atomic_test_and_set((obj), __ATOMIC_SEQ_CST)
#define atomic_flag_clear(obj) __atomic_clear((obj), __ATOMIC_SEQ_CST)

#define atomic_flag_test_and_set_explicit(obj, order) __atomic_test_and_set((obj), order)
#define atomic_flag_clear_explicit(obj, order) __atomic_clear((obj), order)

#define atomic_thread_fence(order) __atomic_thread_fence(order)
#define atomic_signal_fence(order) __atomic_signal_fence(order)

#endif /* _STDATOMIC_H */