#include "idle.hh"
#include "utils.hh"

void idle()
{
    while (1) {
        t_print("Idling...\n");
        asm("hlt");
    };
}