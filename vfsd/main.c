#include <pmos/ipc.h>
#include <stdio.h>
#include <pmos/ports.h>
#include "ports.h"
#include <string.h>
#include <pmos/helpers.h>
#include <stdlib.h>

pmos_port_t main_port = 0;
pmos_port_t devicesd_port = 0;

const char* ps2d_port_name = "/pmos/ps2d";

int main()
{
    printf("Hello from VFSd! My PID: %li\n", getpid());

    return 0;
}