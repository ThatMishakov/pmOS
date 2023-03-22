#pragma once
#include <lib/array.hh>
#include <messaging/messaging.hh>
#include <types.hh>

/** @brief A descriptor of a programmable interrupt.
 * 
 * This descriptor holds a channel for the given interrupt in an interrupts vector.
 * 
 * In a microkernel spirit, none of the interrupts, apart from CPU exceptions, some LAPIC sources (timer) and
 * Inter-Process Interrupts, are serviced in the kernel and instead are dispatched into the userspace. Thus, upon
 * recieving an interrupt, kernel fetches this descriptor from prog_int_array and tries to send the notification
 * to the indicated port. If it is valid and no error has been produced, the listener should then recieve a message
 * containing IPC_Kernel_Interrupt struct indicating the interrupt reason and some additional information. The userspace
 * process is then in charge of dealing with it. The interrupts can be programmed with the help of syscall_set_interrupt()
 * and, although they are thought to be independent, processd is currently in charge of managing the interrupts. 
 * 
 * As currently implemented, the EOI is automatically sent as soon as the interuupt is recieved. The grand idea however is would
 * be to extend the messaging interface to allow the drivers dealing with interrupts to send EOIs themselves as they deem necessary.
 * 
 * @todo Extend messaging interface to let tasks send EOIs
 * @see syscall_set_interrupt()
 * @see programmable_interrupt()
 */
struct Prog_Int_Descriptor {
    klib::weak_ptr<Port> port;
    Spinlock lock;
};

/// The smalles programmable interrupt allowed
constexpr unsigned prog_int_first = 48;

/// The highest programmable intnerrupt allowed
constexpr unsigned prog_inst_last = 247;
constexpr unsigned prog_int_num = prog_inst_last - prog_int_first + 1;

/// Array holding interrupt descriptors, referenced from programmable_interrupt() upon the reception of one.
/// @see Prog_Int_Descriptor
/// @see programmable_interrupt()
extern klib::array<Prog_Int_Descriptor, prog_int_num> prog_int_array;

inline bool intno_is_ok(u32 i)
{
    return i >= prog_int_first and i <= prog_inst_last;
}

inline Prog_Int_Descriptor& get_descriptor(klib::array<Prog_Int_Descriptor, prog_int_num>& array, u32 num)
{
    return array[num - prog_int_first];
}

/// @brief  A function called from assembly upon recieving an interrupt which can be programmed by the userspace.
/// @param intno Interrupt vector number
/// @see Prog_Int_Descriptor
extern "C" void programmable_interrupt(u32 intno);