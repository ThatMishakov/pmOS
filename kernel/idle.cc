#include "idle.hh"
#include "utils.hh"

void idle()
{
    //t_print("Kernel: Idling...\n");

    while (1) {
        asm("hlt");
    };
}