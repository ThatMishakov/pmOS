#include "programmable_ints.hh"
#include <kernel/messaging.h>
#include "apic.hh"

klib::array<Prog_Int_Descriptor, prog_int_num> prog_int_array;


extern "C" void programmable_interrupt(u32 intno)
{
    // t_print_bochs("Interrupt %i\n", intno);

    Kernel_Message_Interrupt kmsg = {KERNEL_MSG_INTERRUPT, intno, get_lapic_id()};

    auto desc = get_descriptor(prog_int_array, intno);

    klib::shared_ptr<Port> port;

    {
        Auto_Lock_Scope scope_lock(desc.lock);
        port = desc.port.lock();
    }

    if (port) {
        port->send_from_system(reinterpret_cast<char*>(&kmsg), sizeof(kmsg));
    }

    apic_eoi();
}

