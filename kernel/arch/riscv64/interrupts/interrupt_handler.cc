#include "interrupt_handler.hh"
#include <exceptions.hh>
#include <kernel/errors.h>
#include <processes/tasks.hh>
#include "plic.hh"
#include <sched/sched.hh>
#include <assert.h>

void Interrupt_Handler_Table::add_handler(u64 interrupt_number, const klib::shared_ptr<Port>& port)
{
    auto c = get_cpu_struct();
    assert(this == &c->int_handlers);
    auto owner = port->owner.lock();
    if (!owner) {
        throw Kern_Exception(ERROR_GENERAL, "Port orphaned");
    }

    if (owner->cpu_affinity != c->cpu_id + 1) {
        throw Kern_Exception(ERROR_GENERAL, "Task not bound to CPU");
    }

    // Check that there isn't a handler already
    if (get_handler(interrupt_number)) {
        throw Kern_Exception(ERROR_GENERAL, "Handler already exists");
    }

    if (interrupt_number >= plic_interrupt_limit())
        throw Kern_Exception(ERROR_GENERAL, "Invalid interrupt number");

    auto handler = klib::make_unique<Interrupt_Handler>();
    handler->interrupt_number = interrupt_number;
    handler->port = port;
    handler->task_id = owner->pid;
    handler->active = true;

    auto handler_ptr = handler.get();

    // Register the handler with the owner
    owner->interrupt_handlers.insert(handler_ptr);

    try {
        handlers.push_back(nullptr);
    } catch (...) {
        owner->interrupt_handlers.erase(handler_ptr);
        throw;
    }

    size_t i = handlers.size() - 1;
    for (; i > 0; i--) {
        if (handlers[i - 1]->interrupt_number < interrupt_number) {
            break;
        } else {
            handlers[i] = klib::move(handlers[i - 1]);
        }
    }

    handlers[i] = klib::move(handler);

    // Enable the interrupt
    plic_interrupt_enable(interrupt_number);
}

Interrupt_Handler *Interrupt_Handler_Table::get_handler(u64 interrupt_number)
{
    // Binary search
    size_t left = 0;
    size_t right = handlers.size();
    while (left < right) {
        size_t mid = (left + right) / 2;
        if (handlers[mid]->interrupt_number < interrupt_number) {
            left = mid + 1;
        } else if (handlers[mid]->interrupt_number > interrupt_number) {
            right = mid;
        } else {
            return handlers[mid].get();
        }
    }

    return nullptr;
}