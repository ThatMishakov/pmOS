#pragma once
#include <lib/memory.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>
#include <utility>

struct CPU_Info;
struct Interrupt_Handler {
    u64 interrupt_number;

    // Cached from port->owner
    u64 task_id;
    Port *port;

    bool active = false;
};

// Per-CPU
struct Interrupt_Handler_Table {
    // Sorted by interrupt number
    klib::vector<klib::unique_ptr<Interrupt_Handler>> handlers;
    size_t allocated_int_count = 0; // Used to distribute interrupts between processors


    [[nodiscard]] kresult_t add_handler(u64 interrupt_number, Port *port);
    kresult_t remove_handler(u64 interrupt_number);
    [[nodiscard]] kresult_t handle_interrupt(u64 interrupt_number);
    [[nodiscard]] kresult_t ack_interrupt(u64 interrupt_number, u64 task);

    Interrupt_Handler *get_handler(u64 interrupt_number);
    size_t get_handler_index(u64 interrupt_number);
};

// Arch-specific functions
kresult_t interrupt_enable(u32 interrupt_id);
void interrupt_disable(u32 interrupt_id);
void interrupt_complete(u32 interrupt_id);

u32 interrupt_min();
u32 interrupt_limint();

ReturnStr<std::pair<CPU_Info *, u32>> allocate_interrupt_single(u32 gsi, bool edge_triggered = false, bool active_low = false);