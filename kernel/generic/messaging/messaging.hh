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
#include <types.hh>
#include <utils.hh>

extern Spinlock messaging_ports;

struct TaskDescriptor;

struct Generic_Port {
    virtual ~Generic_Port() noexcept = default;
};

struct Message {
    klib::weak_ptr<TaskDescriptor> task_from;
    u64 task_id_from = 0;
    klib::vector<char> content;

    inline size_t size() const { return content.size(); }

    // Returns true if done successfully, false otherwise (e.g. when syscall needs to be repeated)
    bool copy_to_user_buff(char *buff);
};

#define MSG_ATTR_PRESENT   0x01ULL
#define MSG_ATTR_DUMMY     0x02ULL
#define MSG_ATTR_NODEFAULT 0x03ULL

class TaskGroup;

class Port: public Generic_Port
{
public:
    klib::weak_ptr<TaskDescriptor> owner;
    klib::weak_ptr<Port> self;

    Spinlock lock;
    u64 portno;

    Port(const klib::shared_ptr<TaskDescriptor> &owner, u64 portno);

    void enqueue(const klib::shared_ptr<Message> &msg);

    void send_from_system(klib::vector<char> &&msg);
    void send_from_system(const char *msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the
    // syscall). Throws on crytical errors
    bool send_from_user(const klib::shared_ptr<TaskDescriptor> &sender, const char *unsafe_user_ptr,
                        size_t msg_size);
    void atomic_send_from_system(const char *msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the
    // syscall). Throws on crytical errors
    bool atomic_send_from_user(const klib::shared_ptr<TaskDescriptor> &sender,
                               const char *unsafe_user_message, size_t msg_size);

    void change_return_upon_unblock(const klib::shared_ptr<TaskDescriptor> &task);

    klib::shared_ptr<Message> &get_front();
    void pop_front() noexcept;
    bool is_empty() const noexcept;

    /**
     * @brief Destructor of port
     *
     * This destructor cleans up the port and does other misc stuff such as sending not-delivered
     * acknowledgements
     */
    virtual ~Port() noexcept override;

    /**
     * @brief Gets the port or NULL if it doesn't exist
     */
    static klib::shared_ptr<Port> atomic_get_port(u64 portno) noexcept;

    /**
     * @brief Gets the port. Throws ERROR_PORT_DOESNT_EXIST if there is no port
     */
    static klib::shared_ptr<Port> atomic_get_port_throw(u64 portno);

    /**
     * @brief Creates a new port with *task* as its owner
     */
    static klib::shared_ptr<Port> atomic_create_port(const klib::shared_ptr<TaskDescriptor> &task);

protected:
    using Message_storage = klib::list<klib::shared_ptr<Message>>;
    Message_storage msg_queue;

    klib::splay_tree_map<u64, klib::weak_ptr<TaskGroup>> notifier_ports;
    mutable Spinlock notifier_ports_lock;

    static inline u64 biggest_port = 1;
    static inline klib::splay_tree_map<u64, klib::weak_ptr<Port>> ports;
    static inline Spinlock ports_lock;

    friend class TaskGroup;
};