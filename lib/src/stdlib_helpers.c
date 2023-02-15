#include <stdio.h>
#include <stdlib.h>
#include <stdio_internal.h>

void init_malloc();
void _clib_init_stdio();

void init_std_lib()
{
    init_malloc();
    _clib_init_stdio();
}