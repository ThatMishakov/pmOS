#include <ucontext.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>
#include <stddef.h>

void __attribute__((noreturn)) __setcontext_partial(const ucontext_t *ucp);

int setcontext(const ucontext_t *ucp) {
    if (!ucp) {
        errno = EINVAL;
        return -1;
    }

    // Disable signals. They will be restored with the context
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, NULL);


    if (ucp->uc_mcontext.partial) {
        __setcontext_partial(ucp);
    }

    // Not implemented
    assert(false);
    return 0;
}
