#include <pmos/ipc.h>
#include <stdio.h>
#include <pmos/ports.h>
#include "ports.h"
#include <string.h>
#include <pmos/helpers.h>
#include <stdlib.h>

pmos_port_t main_port = 0;

int main()
{
    printf("Hello from VFSd! My PID: %li\n", getpid());

    return 0;
}