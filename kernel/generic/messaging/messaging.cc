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

bool Message::copy_to_user_buff(char *buff)
{
    return copy_to_user(&content.front(), buff, content.size());
}

klib::shared_ptr<Port> Port::atomic_create_port(TaskDescriptor *task)
{
    assert(task);

    u64 new_port = __atomic_add_fetch(&biggest_port, 1, __ATOMIC_SEQ_CST);

    klib::shared_ptr<Port> new_port_ptr = klib::make_shared<Port>(task, new_port);

    {
        Auto_Lock_Scope local_lock(ports_lock);
        ports.insert({new_port, new_port_ptr});
    }

    new_port_ptr->self = new_port_ptr;

    try {
        Auto_Lock_Scope local_lock(task->messaging_lock);
        auto result = task->owned_ports.insert_noexcept(new_port_ptr);
        if (result.first == task->owned_ports.end())
            throw(Kern_Exception(-ENOMEM, "failed to insert port into task's owned ports"));
    } catch (...) {
        Auto_Lock_Scope local_lock(ports_lock);
        ports.erase(new_port);
        throw;
    }

    return new_port_ptr;
}

klib::shared_ptr<Port> Port::atomic_get_port(u64 portno) noexcept
{
    Auto_Lock_Scope scope_lock(ports_lock);

    return ports.get_copy_or_default(portno).lock();
}

klib::shared_ptr<Port> Port::atomic_get_port_throw(u64 portno)
{
    Auto_Lock_Scope scope_lock(ports_lock);
    const auto ptr = ports.get_copy_or_default(portno).lock();

    if (not ptr)
        throw(Kern_Exception(-ENOENT, "requested port does not exist"));

    return ptr;
}

void Port::atomic_send_from_system(const char *msg_ptr, uint64_t size)
{
    Auto_Lock_Scope scope_lock(lock);

    send_from_system(msg_ptr, size);
}

void Port::enqueue(const klib::shared_ptr<Message> &msg)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    msg_queue.push_back(msg);
    unblock_if_needed(owner, self.lock());
}

void Port::send_from_system(klib::vector<char> &&v)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    klib::shared_ptr<Message> ptr =
        klib::make_shared<Message>(0, klib::forward<klib::vector<char>>(v));

    enqueue(ptr);
}

void Port::send_from_system(const char *msg_ptr, uint64_t size)
{
    klib::vector<char> message(size);
    memcpy(&message.front(), msg_ptr, size);
    send_from_system(klib::move(message));
}

bool Port::send_from_user(TaskDescriptor *sender,
                          const char *unsafe_user_ptr, size_t msg_size)
{
    assert(lock.is_locked() && "Spinlock not locked!");

    klib::vector<char> message(msg_size);

    kresult_t result = copy_from_user(&message.front(), (char *)unsafe_user_ptr, msg_size);
    if (not result)
        return result;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(
        sender->task_id, klib::forward<klib::vector<char>>(message));

    enqueue(ptr);

    return true;
}

bool Port::atomic_send_from_user(TaskDescriptor *sender,
                                 const char *unsafe_user_message, size_t msg_size)
{
    klib::vector<char> message(msg_size);

    bool result = copy_from_user(&message.front(), (char *)unsafe_user_message, msg_size);
    if (not result)
        return result;

    klib::shared_ptr<Message> ptr = klib::make_shared<Message>(
        sender->task_id, klib::forward<klib::vector<char>>(message));

    Auto_Lock_Scope scope_lock(lock);

    enqueue(ptr);

    return true;
}

void Port::pop_front() noexcept
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.pop_front();
}

bool Port::is_empty() const noexcept
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.empty();
}

klib::shared_ptr<Message> &Port::get_front()
{
    assert(lock.is_locked() && "Spinlock not locked!");

    return msg_queue.front();
}

Port::~Port() noexcept
{
    for (const auto &p: notifier_ports) {
        const auto &ptr = p.second.lock();
        if (ptr) {
            Auto_Lock_Scope l(ptr->notifier_ports_lock);
            ptr->notifier_ports.erase(portno);
        }
    }

    Auto_Lock_Scope scope_lock(ports_lock);
    ports.erase(portno);
}

Port::Port(TaskDescriptor *owner, u64 portno): owner(owner), portno(portno)
{
}