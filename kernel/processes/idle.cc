#include "idle.hh"
#include <utils.hh>
#include <interrupts/apic.hh>

void idle()
{
    while (1) {
        halt();
    };
}