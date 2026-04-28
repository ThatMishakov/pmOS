#pragma once
#include <interrupts/interrupt_handler.hh>
#include <lib/vector.hh>
#include <sched/sched.hh>
#include <types.hh>
#include <optional>
#include <utility>

namespace kernel::x86::interrupts
{

class IOAPIC;

struct IOAPIC_Handler final: ::kernel::interrupts::InterruptHandler {
    IOAPIC *parent_ioapic = nullptr;

    // 48 .. 240
    u32 lapic_vector = 0;

    // gsi - parent_ioapic->int_base
    u32 ioapic_index = 0;

    // Interrupt params
    bool level_triggered : 1 = false;
    bool active_low : 1 = false;
    bool enabled : 1 = false;

    void mask_interrupt();
};

class IOAPIC
{
public:
    void interrupt_enable(u32 vector);
    void interrupt_disable(u32 vector);

    static void init_ioapics();
    static ReturnStr<IOAPIC_Handler *>
        allocate_or_get_handler(u32 gsi, bool edge_triggered, bool active_low);

    static void mask_interrupt(sched::CPU_Info *cpu, int vec);
    
private:

    IOAPIC() = default;

    u32 phys_addr;
    u32 int_base;
    u32 *virt_addr;
    klib::vector<IOAPIC_Handler *> mappings;
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

    static std::optional<std::pair<IOAPIC *, int>> find_ioapic(sched::CPU_Info *cpu, u32 vector);
};

}; // namespace kernel::x86::interrupts