/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "messaging.hh"

#include <assert.h>
#include <errno.h>
#include <kernel/block.h>
#include <lib/utility.hh>
#include <processes/syscalls.hh>
#include <processes/task_group.hh>
#include <sched/sched.hh>
#include <stddef.h>
#include <types.hh>
#include <utils.hh>

ReturnStr<bool> Message::copy_to_user_buff(char *buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}

Port *Port::atomic_create_port(TaskDescriptor *task) noexcept
{
    assert(task);

    u64 new_port = __atomic_add_fetch(&biggest_port, 1, __ATOMIC_SEQ_CST);

    klib::unique_ptr<Port> new_port_ptr = new Port(task, new_port);
    // nothrow?

    // Lock it here so that alive is as it is
    Auto_Lock_Scope scope_lock(new_port_ptr->lock);

    {
        Auto_Lock_Scope scope_lock(task->sched_lock);
        if (task->status == TaskStatus::TASK_DYING || task->status == TaskStatus::TASK_DEAD)
            return nullptr;
        task->owned_ports.insert(new_port_ptr.get());
    }

    {
        Auto_Lock_Scope scope_lock(ports_lock);
        ports.insert(new_port_ptr.get());
    }

    return new_port_ptr.release();
}

Port *Port::atomic_get_port(u64 portno) noexcept
{
    Auto_Lock_Scope lock(ports_lock);
    return ports.find(portno);
}

kresult_t Port::atomic_send_from_system(const char *msg_ptr, uint64_t size)
{
    Auto_Lock_Scope scope_lock(lock);
    return send_from_system(msg_ptr, size);
}

kresult_t Port::enqueue(klib::unique_ptr<Message> msg)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    msg_queue.push_back(msg.release());

    unblock_if_needed(owner, this);
    return 0;
}

kresult_t Port::send_from_system(klib::vector<char> &&v)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    auto ptr =
        klib::make_unique<Message>(0, klib::forward<klib::vector<char>>(v));
    if (!ptr)
        return -ENOMEM;

    return enqueue(klib::move(ptr));
}

kresult_t Port::send_from_system(const char *msg_ptr, uint64_t size)
{
    assert(size > 0);

    klib::vector<char> message;
    if (!message.resize(size))
        return -ENOMEM;

    memcpy(&message.front(), msg_ptr, size);
    return send_from_system(klib::move(message));
}

ReturnStr<bool> Port::send_from_user(TaskDescriptor *sender, const char *unsafe_user_ptr, size_t msg_size)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    klib::vector<char> message;
    if (!message.resize(msg_size))
        return Error(-ENOMEM);

    auto result = copy_from_user(&message.front(), (char *)unsafe_user_ptr, msg_size);
    if (!result.success() || !result.val)
        return result;

    auto ptr =
        klib::make_unique<Message>(sender->task_id, klib::forward<klib::vector<char>>(message));
    if (!ptr)
        return Error(-ENOMEM);

    auto r = enqueue(klib::move(ptr));
    if (not r)
        return Error(r);

    return true;
}

ReturnStr<bool> Port::atomic_send_from_user(TaskDescriptor *sender, const char *unsafe_user_message,
                                 size_t msg_size)
{
    klib::vector<char> message;
    if (!message.resize(msg_size))
        return Error(-ENOMEM);

    auto result = copy_from_user(&message.front(), (char *)unsafe_user_message, msg_size);
    if (!result.success() || !result.val)
        return result;

    auto ptr =
        klib::make_unique<Message>(sender->task_id, klib::forward<klib::vector<char>>(message));
    if (!ptr)
        return Error(-ENOMEM);

    Auto_Lock_Scope scope_lock(lock);

    auto r = enqueue(klib::move(ptr));
    if (not r)
        return Error(r);

    return true;
}

void Port::pop_front() noexcept
{
    assert(lock.is_locked() && "Spinlock not locked!");
    assert(not msg_queue.empty());
    auto begin = msg_queue.begin();
    msg_queue.remove(begin);
    delete &*begin;
}

bool Port::is_empty() const noexcept
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.empty();
}

Message *Port::get_front()
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return &msg_queue.front();
}

bool Port::delete_self() noexcept
{
    {
        Auto_Lock_Scope scope_lock(lock);
        if (not alive)
            return false;
        alive = false;
    }

    {
        Auto_Lock_Scope scope_lock(owner->sched_lock);
        owner->owned_ports.erase(this);
    }

    {
        Auto_Lock_Scope scope_lock(ports_lock);
        ports.erase(this);
    }

    for (const auto &p: notifier_ports) {
        const auto ptr = p.second.lock();
        if (ptr) {
            Auto_Lock_Scope l(ptr->notifier_ports_lock);
            ptr->notifier_ports.erase(portno);
        }
    }

    rcu_head.rcu_func = [](void *self, bool) {
        Port *t =
            reinterpret_cast<Port *>(reinterpret_cast<char *>(self) - offsetof(Port, rcu_head));
        delete t;
    };
    get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);

    return true;
}

Port::~Port() noexcept {}

Port::Port(TaskDescriptor *owner, u64 portno): owner(owner), portno(portno) {}