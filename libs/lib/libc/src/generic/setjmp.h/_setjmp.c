#include <setjmp.h>

int _setjmp(jmp_buf env) { return sigsetjmp(env, 0); }