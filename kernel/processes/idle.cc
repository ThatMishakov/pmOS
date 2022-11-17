#include "idle.hh"
#include <utils.hh>
#include <interrupts/apic.hh>

void idle()
{
    t_print("Kernel: CPU %i idling for the first time...\n", get_lapic_id() >> 24);

    while (1) {
        asm("hlt");
    };
}