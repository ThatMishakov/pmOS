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

#pragma once
#include <lib/memory.hh>
#include <types.hh>

namespace kernel
{

namespace proc
{
    class TaskDescriptor;
}

namespace sched
{

    /**
     * @brief Sheduler queue of tasks
     *
     * This structure is a queue of tasks, used for scheduling. It is used to store queues of
     * processes in a different states (ready, blocked, uninited). The process can only be in 1
     * queue at the time (or in no queue if it is running). The queue is implemented as a
     * doubly-linked list with the pointers inside task descriptor.
     *
     * The kernel uses one queue for the processes that are uninited. The ready processes are stored
     * in different multilevel queues (currently, there are 16 levels), one global and one per CPU,
     * allowing for processes to be bound to particular CPUs. The blocked processes are stored in
     * local blocked queues.
     *
     * The functions thet are not prefixed by "atomic_" offer no protection against race conditions.
     * If the structure might be accesed by different CPUs at the same time, the *lock* should be
     * locked before calling them.
     */
    class sched_queue
    {
    public:
        /**
         * Pushes the task descriptor to the front of the queue and sets its parrent queue to
         * *this*. The function does not check if the task is in other queue. Caller should first
         * make sure it isn't to not corrupt other queue's state.
         */
        void push_front(proc::TaskDescriptor *desc) noexcept;

        /**
         * Same as push_front(), except that the task is pushed to the back of the queue.
         *
         * @see push_front()
         */
        void push_back(proc::TaskDescriptor *desc) noexcept;

        /**
         * Erases the descriptor from the queue. Set's descriptor's *parent_queue* to nullptr.
         */
        void erase(proc::TaskDescriptor *desc) noexcept;

        /**
         * @brief Erases *desc* from the given queue
         *
         * Same as erase(), except it protects the queue with a spinlock
         *
         * @param desc Task to be erased from the queue. Must be valid and pertain to the queue.
         * Does not do errors checking
         */
        void atomic_erase(proc::TaskDescriptor *desc) noexcept;

        // Returns front task or null pointer if empty
        proc::TaskDescriptor *pop_front() noexcept;

        /// Returns true if the queue is empty
        bool is_empty() noexcept;

        /// Spinlock to protect the queue against concurrent execution issues
        Spinlock lock;

        /// Default contstructor
        constexpr sched_queue() noexcept = default;

        /// Queue destructor. Before the destruction, deletes all the tasks from the queue
        virtual ~sched_queue() noexcept;

        /// Returns the first task in the queue
        proc::TaskDescriptor *front() const noexcept;

    protected:
        proc::TaskDescriptor *first = nullptr;
        proc::TaskDescriptor *last  = nullptr;

        /// Delete copy constructor. Copying of the queue is most likely an error
        sched_queue(const sched_queue &) = delete;

        /// Delete move contructor. Since the tasks inside the queue hold pointers to parent queue,
        /// it makes it complicated to move it.
        sched_queue(sched_queue &&) = delete;
    };

    /// Queue for the blocked processes. The same as the normal sched_queue, except that upon
    /// deletion it automatically unblocks all the tasks
    class blocked_sched_queue final: public sched_queue
    {
    public:
        /// Before destroying the queue, calls unlock() on all the tasks, pushing them to ready
        ~blocked_sched_queue() noexcept;
    };

} // namespace sched
} // namespace kernel