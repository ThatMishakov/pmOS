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

#include "sched_queue.hh"

#include "assert.h"

#include <processes/tasks.hh>

klib::shared_ptr<TaskDescriptor> sched_queue::pop_front() noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    klib::shared_ptr<TaskDescriptor> ptr = first;

    if (first != klib::shared_ptr<TaskDescriptor>(nullptr))
        erase(ptr);

    return ptr;
}

klib::shared_ptr<TaskDescriptor> sched_queue::front() const noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    return first;
}

void sched_queue::push_back(const klib::shared_ptr<TaskDescriptor> &desc) noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    if (first == klib::shared_ptr<TaskDescriptor>(nullptr)) {
        first            = desc;
        last             = desc;
        desc->queue_prev = nullptr;
    } else {
        last->queue_next = desc;
        desc->queue_prev = last;
        last             = desc;
    }

    desc->queue_next   = nullptr;
    desc->parent_queue = this;
}

void sched_queue::push_front(const klib::shared_ptr<TaskDescriptor> &desc) noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    if (first == klib::shared_ptr<TaskDescriptor>(nullptr)) {
        first            = desc;
        last             = desc;
        desc->queue_next = nullptr;
    } else {
        desc->queue_next  = first;
        first->queue_prev = desc;
        first             = desc;
    }

    desc->queue_prev   = nullptr;
    desc->parent_queue = this;
}

void sched_queue::erase(const klib::shared_ptr<TaskDescriptor> &desc) noexcept
{
    assert(lock.is_locked() and "Queue is not locked!");

    if (desc->queue_prev) {
        desc->queue_prev->queue_next = desc->queue_next;
    } else {
        first = desc->queue_next;
    }

    if (desc->queue_next) {
        desc->queue_next->queue_prev = desc->queue_prev;
    } else {
        last = desc->queue_prev;
    }

    desc->queue_prev   = nullptr;
    desc->queue_next   = nullptr;
    desc->parent_queue = nullptr;
}

void sched_queue::atomic_erase(const klib::shared_ptr<TaskDescriptor> &desc) noexcept
{
    {
        Auto_Lock_Scope l(lock);
        if (desc->queue_prev) {
            desc->queue_prev->queue_next = desc->queue_next;
        } else {
            first = desc->queue_next;
        }

        if (desc->queue_next) {
            desc->queue_next->queue_prev = desc->queue_prev;
        } else {
            last = desc->queue_prev;
        }
    }

    desc->queue_prev   = nullptr;
    desc->queue_next   = nullptr;
    desc->parent_queue = nullptr;
}

sched_queue::~sched_queue() noexcept
{
    Auto_Lock_Scope l(lock);

    // If there are still tasks in the queue, something fishy is probably going on... But this might
    // be useful in the future
    while (first)
        erase(first);
}

blocked_sched_queue::~blocked_sched_queue() noexcept
{
    // Try and erase tasks one at a time
    while (true) {
        task_ptr p;

        // Lock the queue and try to get the first task.
        {
            Auto_Lock_Scope l(lock);
            p = first;
        }
        // The queue can't be blocked forever since even though its destructor has been called, as
        // long as it is not empty, someone can still try to erase the tasks from here

        // If p is empty, there are no tasks in the queue. Since it is a destructor, if somebody
        // tries to push tasks to the queue, there is a fat use-after-free problem somewhere... Only
        // tasks that are in the queue should hold pointers to it at this time
        if (not p)
            break;

        // Try to erase the task from the queue without blocking it. This function is ok to fail
        // because other CPUs might have already erased the process from the queue. If we block the
        // queue and then call erase() blocking the task's scheduling lock, we might get a deadlock
        // if someone decides to call atomic_kill() or something similar.
        p->atomic_erase_from_queue(this);
    }
}