#include "timers.hh"
#include <pmos/ipc.h>

namespace kernel::sched
{

size_t TimerRight::size() const
{
    return sizeof(IPC_Timer_Expired);
}

ReturnStr<bool> TimerRight::copy_to_user_buff(char *buff) const
{
    IPC_Timer_Expired msg = {
        .type = IPC_Timer_Expired_NUM,
        .flags = 0,
        .deadline = fire_at_ns,
    };

    return copy_to_user(reinterpret_cast<const char *>(&msg), buff, sizeof(msg));
}

ReturnStr<TimerRight *> TimerRight::create_for_port(ipc::Port *port)
{
    assert(port);

    auto new_timer = klib::make_unique<TimerRight>();
    if (!new_timer)
        return Error(-ENOMEM);

    new_timer->parent = port;

    Auto_Lock_Scope l(port->lock);
    if (!port->atomic_alive())
        return Error(-ENOENT);

    new_timer->right_parent_id = port->new_right_id();
    port->atomic_add_to_rights(new_timer.get());

    return Success(new_timer.release());
}

ipc::RightType TimerRight::recieve_type() const
{
    return ipc::RightType::Timer;
}

static void delete_timer_right(TimerRight *t)
{
    t->rcu_head.rcu_func = [](void *self, bool) {
        TimerRight *t =
            reinterpret_cast<TimerRight *>(reinterpret_cast<char *>(self) - offsetof(TimerRight, rcu_head));
        delete t;
    };
    sched::get_cpu_struct()->heap_rcu_cpu.push(&t->rcu_head);
}

bool TimerRight::should_delete_self() const
{
    return !alive && !waiting_for_attention && !in_timer_queue && !is_sent;
}

void TimerRight::fire()
{
    Auto_Lock_Scope l(lock);
    assert(in_timer_queue);
    in_timer_queue = false;
 
    assert(parent_cpu == get_cpu_struct());
    if (waiting_for_attention) {
        Auto_Lock_Scope scope_lock(parent_cpu->attention_queue_lock);
        parent_cpu->attention_queue.erase(this);
        waiting_for_attention = false;
    }

    if (!alive) {
        if (should_delete_self())
            delete_timer_right(this);
        return;
    }

    assert(parent);

    Auto_Lock_Scope port_lock(parent->lock);
    is_sent = true;
    parent->enqueue(klib::unique_ptr<TimerRight>(this));
}

void TimerRight::get_attention()
{
    Auto_Lock_Scope l(lock);
    assert(waiting_for_attention);
    waiting_for_attention = false;

    assert(parent_cpu == get_cpu_struct());

    {
        Auto_Lock_Scope port_lock(parent_cpu->attention_queue_lock);
        parent_cpu->attention_queue.erase(this);
    }

    assert(in_timer_queue);

    parent_cpu->timer_queue.erase(this);
    in_timer_queue = false;

    if (alive && next_update_ns) {
        fire_at_ns = next_update_ns;
        parent_cpu->timer_queue.insert(this);
        maybe_rearm_timer(next_update_ns);
        next_update_ns = 0;
    }

    if (!alive && should_delete_self())
        delete_timer_right(this);
}

void TimerRight::delete_self()
{
    Auto_Lock_Scope l(lock);
    assert(is_sent);
    is_sent = false;

    assert(!waiting_for_attention);
    assert(!in_timer_queue);
    assert(parent);

    if (!alive) {
        assert(should_delete_self());
        delete_timer_right(this);
        return;
    }

    if (next_update_ns) {
        parent_cpu = get_cpu_struct();
        fire_at_ns = next_update_ns;
        in_timer_queue = true;
        parent_cpu->timer_queue.insert(this);
        maybe_rearm_timer(next_update_ns);
        next_update_ns = 0;
    }
}

bool TimerRight::destroy_recieve_right()
{
    Auto_Lock_Scope l(lock);
    if (!alive)
        return false;

    alive = false;

    assert(parent);
    parent->atomic_remove_right(this);

    if (in_timer_queue) {
        if (parent_cpu == get_cpu_struct()) {
            parent_cpu->timer_queue.erase(this);
            in_timer_queue = false;
        
            if (waiting_for_attention) {
                Auto_Lock_Scope scope_lock(parent_cpu->attention_queue_lock);
                parent_cpu->attention_queue.erase(this);
                waiting_for_attention = false;
            }
        } else {
            if (!waiting_for_attention)
                request_attention();
        }
    }

    if (should_delete_self())
        delete_timer_right(this);
}

void TimerRight::request_attention()
{
    assert(parent_cpu);
    assert(!waiting_for_attention);
    assert(in_timer_queue);

    assert(lock.is_locked());

    Auto_Lock_Scope scope_lock(parent_cpu->attention_queue_lock);
    parent_cpu->attention_queue.push_back(this);
    waiting_for_attention = true;

    parent_cpu->ipi_get_attention();
}

}