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
#include <lib/splay_tree_map.hh>
#include <messaging/messaging.hh>
#include <types.hh>

class TaskDescriptor;

class TaskGroup: public klib::enable_shared_from_this<TaskGroup>
{
public:
    using id_type = u64;

    ~TaskGroup() noexcept;

    /**
     * @brief Create a task group
     *
     * @return klib::shared_ptr<TaskGroup> New task group
     */
    static klib::shared_ptr<TaskGroup> create();

    /**
     * @brief Get the task group with the given id
     *
     * This function is thread safe
     *
     * @param id Task group id
     * @throws out of range if the task group does not exist
     */
    static klib::shared_ptr<TaskGroup> get_task_group(u64 id);

    /**
     * @brief Checks if the task with the given id is in the group
     *
     * @param id Task id
     * @return true If the task is in the group
     * @return false If the task is not in the group
     */
    bool atomic_has_task(u64 id) const noexcept;

    /**
     * @brief Registers the task with the group
     *
     * This function atomically registers the task with the group, by adding it to the set in the
     * task. It is thread-safe
     *
     * @param task Task to register
     */
    [[nodiscard]] kresult_t atomic_register_task(TaskDescriptor * task);

    /**
     * @brief Removes the task from the group, throwing if not in the group
     *
     * @param task Task to remove
     */
    [[nodiscard]] kresult_t atomic_remove_task(TaskDescriptor *task);

    /**
     * @brief Removes a task from the group. If the task is not in the group, does nothing
     *
     * @param id ID of the task to remove
     * @return true If the task was in the group
     * @return false If the task was not in the group
     */
    [[nodiscard]] kresult_t atomic_remove_task(u64 id) noexcept;

    /**
     * @brief Get the id of this task group
     *
     * @return id_type Task group id
     */
    inline id_type get_id() const noexcept { return id; }

    /**
     * @brief Changes the notifier port to the new mask. Setting mask to 0 effectively removes the
     * port from the notifiers map
     *
     * @param port Port whose mask is to be changed. Must not be nullptr.
     * @param mask New mask
     * @return u64 Old mask
     */
    ReturnStr<u32> atomic_change_notifier_mask(Port *port, u32 mask, u32 flags);

    /**
     * @brief Gets the notification mask of the port. If the mask is 0, then the port is not in the
     * notifiers map.
     *
     * @param port_id ID of the port whose mask is to be checked
     * @return u64 Mas of the port
     */
    ReturnStr<u32> atomic_get_notifier_mask(u64 port_id);

private:
    id_type id = __atomic_fetch_add(&next_id, 1, __ATOMIC_SEQ_CST);

    klib::splay_tree_map<u64, TaskDescriptor*> tasks;
    mutable Spinlock tasks_lock;

    static inline klib::splay_tree_map<u64, klib::weak_ptr<TaskGroup>> global_map;
    static inline Spinlock global_map_lock;

    struct NotifierPort {
        u64 action_mask = 0;

        static constexpr u64 ACTION_MASK_ON_DESTROY     = 0x01;
        static constexpr u64 ACTION_MASK_ON_REMOVE_TASK = 0x02;
        static constexpr u64 ACTION_MASK_ON_ADD_TASK    = 0x04;

        static constexpr u64 ACTION_MASK_ALL = 0x07;
    };

    klib::splay_tree_map<u64, NotifierPort> notifier_ports;
    mutable Spinlock notifier_ports_lock;

    TaskGroup() = default;

    /**
     * @brief Adds this task group to the global map
     */
    void atomic_add_to_global_map();

    /**
     * @brief Removes this task group from the global map
     */
    void atomic_remove_from_global_map() const noexcept;

    static inline u64 next_id = 1;

    friend bool Port::delete_self() noexcept;
};