#include <setjmp.h>

int setjmp(jmp_buf env) { return sigsetjmp(env, 1); }