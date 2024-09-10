#include "interrupt_handler.hh"

#include <assert.h>
#include <errno.h>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>
#include <sched/sched.hh>
void Interrupt_Handler_Table::add_handler(u64 interrupt_number, Port *port)
{
    auto c = get_cpu_struct();
    assert(this == &c->int_handlers);
    auto owner = port->owner;

    // Check that there isn't a handler already
    if (get_handler(interrupt_number)) {
        throw Kern_Exception(-EEXIST, "Handler already exists");
    }

    if (interrupt_number > interrupt_max() || interrupt_number < interrupt_min()) {
        serial_logger.printf("Interrupt number: %d\n", interrupt_number);
        throw Kern_Exception(-EINVAL, "Invalid interrupt number");
    }

    auto handler              = klib::make_unique<Interrupt_Handler>();
    handler->interrupt_number = interrupt_number;
    handler->port             = port;
    handler->task_id          = owner->task_id;
    handler->active           = true;

    auto handler_ptr = handler.get();

    {
        Auto_Lock_Scope lock(owner->sched_lock);
        if (owner->status == TaskStatus::TASK_DYING || owner->status == TaskStatus::TASK_DEAD) {
            throw Kern_Exception(-ESRCH, "Task is dying or dead");
        }

        if (owner->cpu_affinity != c->cpu_id + 1) {
            throw Kern_Exception(-EINVAL, "Task not bound to CPU");
        }

        // Register the handler with the owner
        auto result = owner->interrupt_handlers.insert_noexcept(handler_ptr);
        if (!result.first)
            throw Kern_Exception(-ENOMEM, "not enough memory to insert the interrupt handler");
    }

    auto on_error = pmos::utility::make_scope_guard([&] {
        Auto_Lock_Scope lock(owner->sched_lock);
        owner->interrupt_handlers.erase(handler_ptr);
    });

    handlers.push_back(nullptr);

    size_t i = handlers.size() - 1;
    for (; i > 0; i--) {
        if (handlers[i - 1]->interrupt_number < interrupt_number) {
            break;
        } else {
            handlers[i] = klib::move(handlers[i - 1]);
        }
    }

    handlers[i] = klib::move(handler);

    on_error.dismiss();

    // Enable the interrupt
    interrupt_enable(interrupt_number);
}

Interrupt_Handler *Interrupt_Handler_Table::get_handler(u64 interrupt_number)
{
    auto i = get_handler_index(interrupt_number);
    if (i == handlers.size()) {
        return nullptr;
    }

    return handlers[i].get();
}

size_t Interrupt_Handler_Table::get_handler_index(u64 interrupt_number)
{
    // Binary search
    size_t left  = 0;
    size_t right = handlers.size();
    while (left < right) {
        size_t mid = (left + right) / 2;
        if (handlers[mid]->interrupt_number < interrupt_number) {
            left = mid + 1;
        } else if (handlers[mid]->interrupt_number > interrupt_number) {
            right = mid;
        } else {
            return mid;
        }
    }

    return handlers.size();
}

void Interrupt_Handler_Table::remove_handler(u64 interrupt_number)
{
    auto c = get_cpu_struct();
    assert(this == &c->int_handlers);
    auto handler = get_handler(interrupt_number);
    if (!handler) {
        throw Kern_Exception(-ESRCH, "Handler does not exist");
    }

    auto owner = get_task(handler->task_id);

    if (owner)
        owner->interrupt_handlers.erase(handler);

    // Disable the interrupt
    interrupt_disable(interrupt_number);

    // Find the handler index
    auto handler_index = get_handler_index(interrupt_number);

    // Remove the handler
    for (size_t i = handler_index + 1; i < handlers.size(); i++) {
        handlers[i - 1] = klib::move(handlers[i]);
    }
    handlers.pop_back();
}

void Interrupt_Handler_Table::ack_interrupt(u64 interrupt_number, u64 task)
{
    auto handler = get_handler(interrupt_number);
    if (!handler)
        throw Kern_Exception(-ESRCH, "Handler does not exist");

    if (handler->task_id != task)
        throw Kern_Exception(-EPERM, "Task does not own handler");

    if (!handler->active)
        throw Kern_Exception(-EBADF, "Handler not active");

    handler->active = false;
    interrupt_complete(interrupt_number);
}