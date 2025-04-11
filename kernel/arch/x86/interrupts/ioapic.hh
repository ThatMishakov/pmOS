#pragma once
#include <interrupts/interrupt_handler.hh>
#include <lib/vector.hh>
#include <sched/sched.hh>
#include <types.hh>

namespace kernel::x86::interrupts
{

class IOAPIC
{
public:
    void interrupt_enable(u32 vector);
    void interrupt_disable(u32 vector);

    static void init_ioapics();
    static ReturnStr<std::pair<sched::CPU_Info *, u32>>
        allocate_interrupt_single(u32 gsi, bool edge_triggered, bool active_low);

private:
    struct IntMapping {
        sched::CPU_Info *mapped_to {};
        u32 vector {};
    };

    IOAPIC() = default;

    u32 phys_addr;
    u32 int_base;
    u32 *virt_addr;
    klib::vector<IntMapping> mappings;
    u32 int_count;
    unsigned ioapic_id;
    Spinlock io_lock;

    u32 read_reg(unsigned offset);
    void write_reg(unsigned offset, u32 value);

    u64 read_redit_entry(unsigned idx);
    void write_redir_entry(unsigned idx, u64 value);

    static IOAPIC *get_ioapic(u32 gsi);

    enum regs : unsigned {
        IOAPICID  = 0,
        IOAPICVER = 1,
        IOAPICARB = 2,

        IOREDTBL0 = 0x10,
    };

    void push_global();
};

}; // namespace kernel::x86::interrupts