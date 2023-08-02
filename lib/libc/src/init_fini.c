#include <stdlib.h>

void _init(void);
void _fini(void);

extern void (*__preinit_array_start[])(void);
extern void (*__preinit_array_end[])(void);

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

void __call_init_functions(void)
{
    for (size_t i = 0; __preinit_array_start[i] < __preinit_array_end[0]; i++) {
        __preinit_array_start[i]();
    }

    _init();

    for (size_t i = 0; __init_array_start[i] < __init_array_end[0]; i++) {
        __init_array_start[i]();
    }
}

void __call_destructors(void)
{
    // The following code potentially produces compiler bug in GCC 12 by incrementing fini_func by 1 byte in assembly
    // or I am too stupid to see the bigger picture.
    // void (*fini_func)(void) = __fini_array_start[0];
    // while (fini_func != __fini_array_end[0]) {
    //     fini_func();
    //     fini_func++;
    // }


    // I don't know which one needs to be called first

    for (size_t i = 0; __fini_array_start[i] < __fini_array_end[0]; i++) {
        __fini_array_start[i]();
    }

    _fini();
}