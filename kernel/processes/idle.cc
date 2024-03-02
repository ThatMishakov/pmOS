#include "idle.hh"
#include <kern_logger/kern_logger.hh>

void idle()
{
    serial_logger.printf("Idle task entered for the first time!\n");

    while (1) {
        halt();
    };
}