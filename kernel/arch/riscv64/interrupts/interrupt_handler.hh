#pragma once
#include <lib/memory.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>

struct Interrupt_Handler {
    u64 interrupt_number;

    // Cached from port->owner
    u64 task_id;
    klib::weak_ptr<Port> port;

    bool active = false;
};

struct Interrupt_Handler_Table {
    // Sorted by interrupt number
    klib::vector<klib::unique_ptr<Interrupt_Handler>> handlers;

    void add_handler(u64 interrupt_number, const klib::shared_ptr<Port> &port);
    void remove_handler(u64 interrupt_number);
    void handle_interrupt(u64 interrupt_number);
    void ack_interrupt(u64 interrupt_number, u64 task);

    Interrupt_Handler *get_handler(u64 interrupt_number);
    size_t get_handler_index(u64 interrupt_number);
};