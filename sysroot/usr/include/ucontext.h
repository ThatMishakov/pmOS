#ifndef __UCONTEXT_H
#define __UCONTEXT_H 1

#define __DECLARE_SIGSET_T
#define __DECLARE_STACK_T
#include "__posix_types.h"

typedef unsigned long size_t;


typedef int sigset_t;
typedef void* mcontext_t; // TODO

typedef struct ucontext_t {
    struct ucontext_t *uc_link;
    sigset_t    uc_sigmask;
    stack_t     uc_stack;
    mcontext_t  uc_mcontext;
} ucontext_t;

#if defined(__cplusplus)
extern "C" {
#endif

int  getcontext(ucontext_t *);
int  setcontext(const ucontext_t *);

/**
 * @brief Create a new user context for a coroutine.
 *
 * The `makecontext` function is used to create a new user context for a coroutine.
 * A user context is a container for a set of processor registers and a stack that
 * can be used to execute a function in a separate stack frame.
 *
 * @param ucp      A pointer to the user context to be created.
 * @param func     A pointer to the function that the user context should execute.
 * @param argc     The number of arguments to pass to the function.
 * @param ...      Additional arguments to pass to the function.
 * @return         This function does not return a value.
 *
 * @note           The `makecontext` function is typically used in conjunction with
 *                the `swapcontext` function to implement cooperative multitasking
 *                or coroutines.
 */
void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...);

int  swapcontext(ucontext_t *, const ucontext_t *);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif