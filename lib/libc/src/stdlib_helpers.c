#include <stdio.h>
#include <stdlib.h>
#include <stdio_internal.h>

void _clib_init_stdio();

void init_std_lib()
{
    _clib_init_stdio();
}