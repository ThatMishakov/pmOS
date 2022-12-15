#include <stdio.h>
#include "interrupts.h"

int main(int argc, char *argv[])
{
    printf("Hello from PS2d!\n");

    get_interrupt_number();

    return 0;
}