#include "idle.hh"

void idle()
{
    while (1) {
        halt();
    };
}