#include "idle.hh"
#include "utils.hh"

void idle()
{
    while (1) {
        asm("hlt");
    };
}