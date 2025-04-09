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
#include <lib/list.hh>
#include <lib/memory.hh>
#include <lib/queue.hh>
#include <lib/splay_tree_map.hh>
#include <lib/vector.hh>
#include <memory/rcu.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/containers/map.hh>
#include <types.hh>
#include <utils.hh>

extern Spinlock messaging_ports;

class TaskDescriptor;

struct Message {
    pmos::containers::DoubleListHead<Message> list_node;
    u64 task_id_from  = 0;
    u64 mem_object_id = 0;
    klib::vector<char> content;

    Message(u64 task_id_from, klib::vector<char> content, u64 mem_object_id = 0)
        : task_id_from(task_id_from), mem_object_id(mem_object_id), content(klib::move(content))
    {
    }

    inline size_t size() const { return content.size(); }

    // Returns true if done successfully, false otherwise (e.g. when syscall needs to be repeated)
    ReturnStr<bool> copy_to_user_buff(char *buff);
};

#define MSG_ATTR_PRESENT   0x01ULL
#define MSG_ATTR_DUMMY     0x02ULL
#define MSG_ATTR_NODEFAULT 0x03ULL

class TaskGroup;

class Port
{
public:
    TaskDescriptor *owner;

    Spinlock lock;
    u64 portno;
    bool alive = true;

    Port(TaskDescriptor *owner, u64 portno);

    void enqueue(klib::unique_ptr<Message> msg);

    kresult_t send_from_system(klib::vector<char> &&msg);
    kresult_t send_from_system(const char *msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the
    // syscall). Throws on crytical errors
    ReturnStr<bool> send_from_user(TaskDescriptor *sender, const char *unsafe_user_ptr,
                                   size_t msg_size);
    kresult_t atomic_send_from_system(const char *msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the
    // syscall). Throws on crytical errors
    ReturnStr<bool> atomic_send_from_user(TaskDescriptor *sender, const char *unsafe_user_message,
                                          size_t msg_size, u64 mem_object_id);

    void change_return_upon_unblock(TaskDescriptor *task);

    Message *get_front();
    void pop_front() noexcept;
    bool is_empty() const noexcept;

    /**
     * @brief Destructor of port
     *
     * This destructor cleans up the port and does other misc stuff such as sending not-delivered
     * acknowledgements
     */
    ~Port() noexcept;

    /**
     * @brief Gets the port or NULL if it doesn't exist
     */
    static Port *atomic_get_port(u64 portno) noexcept;

    /**
     * @brief Creates a new port with *task* as its owner
     */
    static Port *atomic_create_port(TaskDescriptor *task) noexcept;

    /// Deletes the port. Returns false if the port is already deleted
    bool delete_self() noexcept;

protected:
    using Message_storage =
        pmos::containers::CircularDoubleList<Message, &Message::list_node>;
    Message_storage msg_queue;

    union {
        pmos::containers::RBTreeNode<Port> bst_head_global;
        RCU_Head rcu_head;
    };
    pmos::containers::RBTreeNode<Port> bst_head_owner;

    pmos::containers::map<u64, klib::weak_ptr<TaskGroup>> notifier_ports;
    mutable Spinlock notifier_ports_lock;

    using global_ports_tree =
        pmos::containers::RedBlackTree<Port, &Port::bst_head_global,
                                       detail::TreeCmp<Port, u64, &Port::portno>>;

    static inline u64 biggest_port = 1;
    static inline global_ports_tree::RBTreeHead ports;
    static inline Spinlock ports_lock;

    friend class TaskGroup;
    friend class TaskDescriptor;
};

kresult_t set_port0(Port *port);