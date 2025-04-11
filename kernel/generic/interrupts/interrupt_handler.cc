#include "interrupt_handler.hh"

#include <assert.h>
#include <errno.h>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <pmos/utility/scope_guard.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>

using namespace kernel::interrupts;

kresult_t Interrupt_Handler_Table::add_handler(u64 interrupt_number, ipc::Port *port)
{
    auto c = sched::get_cpu_struct();
    assert(this == &c->int_handlers);
    auto owner = port->owner;

    // Check that there isn't a handler already
    // TODO: Allow interrupt sharing...
    if (get_handler(interrupt_number))
        return -EEXIST;

    if (interrupt_number >= interrupt_limint() || interrupt_number < interrupt_min()) {
        log::serial_logger.printf("Interrupt number %d invalid\n", interrupt_number);
        return -EINVAL;
    }

    auto handler              = klib::make_unique<Interrupt_Handler>();
    handler->interrupt_number = interrupt_number;
    handler->port             = port;
    handler->task_id          = owner->task_id;
    handler->active           = true;

    auto handler_ptr = handler.get();

    {
        Auto_Lock_Scope lock(owner->sched_lock);
        if (owner->status == proc::TaskStatus::TASK_DYING || owner->status == proc::TaskStatus::TASK_DEAD)
            return -ESRCH;

        if (owner->cpu_affinity != c->cpu_id + 1) {
            log::serial_logger.printf("Task %d (%s) is not bound to CPU %d\n", owner->task_id,
                                      owner->name.c_str(), c->cpu_id);
            return -EPERM;
        }

        // Register the handler with the owner
        auto result = owner->interrupt_handlers.insert_noexcept(handler_ptr);
        if (!result.first)
            return -ENOMEM;
    }

    auto on_error = pmos::utility::make_scope_guard([&] {
        Auto_Lock_Scope lock(owner->sched_lock);
        owner->interrupt_handlers.erase(handler_ptr);
    });

    if (!handlers.push_back(nullptr))
        return -ENOMEM;

    // Enable the interrupt
    auto result = interrupt_enable(interrupt_number);
    if (result) {
        handlers.pop_back();
        return result;
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

    on_error.dismiss();

    return 0;
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

kresult_t Interrupt_Handler_Table::remove_handler(u64 interrupt_number)
{
    auto c = sched::get_cpu_struct();
    assert(this == &c->int_handlers);
    auto handler = get_handler(interrupt_number);
    if (!handler)
        return -ESRCH;

    auto owner = proc::get_task(handler->task_id);

    if (owner)
        owner->interrupt_handlers.erase(handler);

    // Disable the interrupt
    interrupt_disable(interrupt_number);

    if (handler->active)
        interrupt_complete(interrupt_number);

    // Find the handler index
    auto handler_index = get_handler_index(interrupt_number);

    // Remove the handler
    for (size_t i = handler_index + 1; i < handlers.size(); i++) {
        handlers[i - 1] = klib::move(handlers[i]);
    }
    handlers.pop_back();

    return 0;
}

kresult_t Interrupt_Handler_Table::ack_interrupt(u64 interrupt_number, u64 task)
{
    auto handler = get_handler(interrupt_number);
    if (!handler)
        return -ESRCH;

    if (handler->task_id != task)
        return -EPERM;

    if (!handler->active)
        return -EBADF;

    handler->active = false;
    interrupt_complete(handler->interrupt_number);

    return 0;
}