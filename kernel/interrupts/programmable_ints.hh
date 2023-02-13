#pragma once
#include <lib/array.hh>
#include <messaging/messaging.hh>
#include <types.hh>

struct Prog_Int_Descriptor {
    klib::weak_ptr<Port> port;
    Spinlock lock;

    kresult_t notify_of_int();
};

constexpr unsigned prog_int_first = 48;
constexpr unsigned prog_inst_last = 247;
constexpr unsigned prog_int_num = prog_inst_last - prog_int_first + 1;

extern klib::array<Prog_Int_Descriptor, prog_int_num> prog_int_array;

kresult_t notify_of_int(unsigned intno);

inline bool intno_is_ok(u32 i)
{
    return i >= prog_int_first and i <= prog_inst_last;
}

inline Prog_Int_Descriptor& get_descriptor(klib::array<Prog_Int_Descriptor, prog_int_num>& array, u32 num)
{
    return array[num - prog_int_first];
}

extern "C" void programmable_interrupt(u32 intno);