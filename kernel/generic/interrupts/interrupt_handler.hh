#pragma once
#include <lib/memory.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>

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

    [[nodiscard]] kresult_t add_handler(u64 interrupt_number, Port *port);
    kresult_t remove_handler(u64 interrupt_number);
    [[nodiscard]] kresult_t handle_interrupt(u64 interrupt_number);
    [[nodiscard]] kresult_t ack_interrupt(u64 interrupt_number, u64 task);

    Interrupt_Handler *get_handler(u64 interrupt_number);
    size_t get_handler_index(u64 interrupt_number);
};

// Arch-specific functions
void interrupt_enable(u32 interrupt_id);
void interrupt_disable(u32 interrupt_id);
void interrupt_complete(u32 interrupt_id);

u32 interrupt_min();
u32 interrupt_max();