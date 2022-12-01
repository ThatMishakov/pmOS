#include <stdio.h>
#include <stdlib.h>
#include <stdio_internal.h>

void init_malloc();

static void init_stdio()
{
    init_malloc();

    stdout = malloc(sizeof(FILE));
    stdout->type = _STDIO_FILE_TYPE_PORT;
    stdout->port_ptr = 1;
    stdout->flags = 0;
    stdout->size = 0;

    // TODO
    stderr = stdout;
}

void init_std_lib()
{
    init_stdio();
}