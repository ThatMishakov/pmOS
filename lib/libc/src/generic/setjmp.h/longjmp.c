#include <setjmp.h>

void longjmp(jmp_buf env, int val) { siglongjmp(env, val); }