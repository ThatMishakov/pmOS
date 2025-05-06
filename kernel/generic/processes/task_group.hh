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
#include <memory/rcu.hh>
#include <messaging/messaging.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <types.hh>
#include <messaging/rights.hh>

namespace kernel::ipc {
    class Port;
}

namespace kernel::proc
{

class TaskDescriptor;
class TaskGroup;

class TaskGroup
{
public:
    using id_type = u64;

    /**
     * @brief Create a task group
     *
     * @return klib::shared_ptr<TaskGroup> New task group
     */
    static ReturnStr<TaskGroup *> create_for_task(TaskDescriptor *task);

    /**
     * @brief Get the task group with the given id
     *
     * This function is thread safe
     *
     * @param id Task group id
     * @throws out of range if the task group does not exist
     */
    static TaskGroup *get_task_group(u64 id);

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
    [[nodiscard]] kresult_t atomic_register_task(TaskDescriptor *task);

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
    ReturnStr<u32> atomic_change_notifier_mask(ipc::Port *port, u32 mask, u32 flags);

    /**
     * @brief Gets the notification mask of the port. If the mask is 0, then the port is not in the
     * notifiers map.
     *
     * @param port_id ID of the port whose mask is to be checked
     * @return u64 Mas of the port
     */
    ReturnStr<u32> atomic_get_notifier_mask(u64 port_id);

    bool alive() const noexcept;
    bool atomic_alive() const noexcept;

    bool task_in_group(u64 id) const;

    // TODO: make this private...
    klib::splay_tree_map<u64, TaskDescriptor *> tasks;
    mutable Spinlock tasks_lock;

    ipc::Right *atomic_get_right(u64 right_id);
    u64 atomic_new_right_id();

private:
    id_type id = __atomic_fetch_add(&next_id, 1, __ATOMIC_SEQ_CST);

    union {
        pmos::containers::RBTreeNode<TaskGroup> bst_head_global = {};
        memory::RCU_Head rcu_head;
    };

    using global_tree =
        pmos::containers::RedBlackTree<TaskGroup, &TaskGroup::bst_head_global,
                                       detail::TreeCmp<TaskGroup, id_type, &TaskGroup::id>>;

    static inline global_tree::RBTreeHead global_map;
    static inline Spinlock global_map_lock;

    struct NotifierPort {
        u64 action_mask = 0;

        static constexpr u64 ACTION_MASK_ON_DESTROY     = 0x01;
        static constexpr u64 ACTION_MASK_ON_REMOVE_TASK = 0x02;
        static constexpr u64 ACTION_MASK_ON_ADD_TASK    = 0x04;

        static constexpr u64 ACTION_MASK_ALL = 0x07;
    };

    pmos::containers::map<u64, NotifierPort> notifier_ports;
    mutable Spinlock notifier_ports_lock;

    TaskGroup() = default;

    /**
     * @brief Adds this task group to the global map
     */
    void atomic_add_to_global_map();

    /**
     * @brief Removes this task group from the global map
     */
    void atomic_remove_from_global_map() noexcept;

    static inline u64 next_id = 1;

    friend bool ipc::Port::delete_self() noexcept;

    ~TaskGroup() = default;

    void destroy();

public: // Fun!!!
    using rights_tree =
    pmos::containers::RedBlackTree<ipc::Right, &ipc::Right::task_group_head,
                                   detail::TreeCmp<ipc::Right, u64, &ipc::Right::right_sender_id>>;
    rights_tree::RBTreeHead rights;
    mutable Spinlock rights_lock;
    u64 current_right_id = 0;

    friend struct ipc::Right;
    friend class ipc::Port;
};

}; // namespace kernel::proc