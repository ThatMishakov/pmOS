#include <signal.h>
#include <stdio.h>

void psignal(int sig, const char *s)
{
    const char *signame;
    if (sig < 0 || sig >= NSIG) {
        signame = "Unknown signal";
    } else {
        signame = sys_siglist[sig];
    }

    if (s && *s) {
        fprintf(stderr, "%s: %s\n", s, signame);
    } else {
        fprintf(stderr, "%s\n", signame);
    }
}