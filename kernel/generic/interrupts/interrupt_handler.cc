#include "interrupt_handler.hh"

#include <assert.h>
#include <exceptions.hh>
#include <sched/sched.hh>
#include <errno.h>

#include <kern_logger/kern_logger.hh>
void Interrupt_Handler_Table::add_handler(u64 interrupt_number, const klib::shared_ptr<Port> &port)
{
    auto c = get_cpu_struct();
    assert(this == &c->int_handlers);
    auto owner = port->owner.lock();
    if (!owner) {
        throw Kern_Exception(-EIDRM, "Port orphaned");
    }

    if (owner->cpu_affinity != c->cpu_id + 1) {
        throw Kern_Exception(-EINVAL, "Task not bound to CPU");
    }

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