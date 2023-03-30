#ifndef __SETJMP_H
#define __SETJMP_H 1

#if defined(__cplusplus)
extern "C" {
#endif

// TODO: Signal saving & stuff
typedef void* jmp_buf[5];

#define setjmp __builtin_setjmp;

#if defined(__cplusplus)
} /* extern "C" */
#endif

#if defined(__cplusplus)

extern "C" [[noreturn]] void longjmp(jmp_buf env, int val);

#else
// This cannot be a macro
_Noreturn void longjmp(jmp_buf env, int val);
#endif


#endif // __SETJMP_H