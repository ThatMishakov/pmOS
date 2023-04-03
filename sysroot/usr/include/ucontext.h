#ifndef __UCONTEXT_H
#define __UCONTEXT_H 1

// #define __DECLARE_SIGSET_T
// #define __DECLARE_STACK_T
// #include "__posix_types.h"

typedef unsigned long size_t;

typedef struct stack_t {
    void     *ss_sp;
    size_t    ss_size;
    int       ss_flags;
} stack_t;


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
void makecontext(ucontext_t *, (void *)(), int, ...);
int  swapcontext(ucontext_t *, const ucontext_t *);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif