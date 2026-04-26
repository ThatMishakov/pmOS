#pragma once

#include <array>
#include <lib/memory.hh>
#include <lib/splay_tree_map.hh>
#include <memory/rcu.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <pmos/containers/map.hh>
#include <pmos/containers/set.hh>
#include <types.hh>
#include <utils.hh>
#include "messaging.hh"
#include "rights.hh"

namespace kernel::ipc
{

class Port
{
public:
    proc::TaskDescriptor *owner;

    mutable Spinlock lock;
    u64 portno;
    bool alive = true;

    Port(proc::TaskDescriptor *owner, u64 portno);

    void enqueue(klib::unique_ptr<GenericMessage> msg);

    kresult_t send_from_system(klib::vector<char> &&msg);
    kresult_t send_from_system(const char *msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the
    // syscall). Throws on crytical errors
    ReturnStr<bool> send_from_user(proc::TaskDescriptor *sender, const char *unsafe_user_ptr,
                                   size_t msg_size);
    kresult_t atomic_send_from_system(const char *msg, size_t size);

    // Returns true if successfully sent, false otherwise (e.g. when it is needed to repeat the
    // syscall). Throws on crytical errors
    ReturnStr<bool> atomic_send_from_user(proc::TaskDescriptor *sender,
                                          const char *unsafe_user_message, size_t msg_size);

    void change_return_upon_unblock(proc::TaskDescriptor *task);

    GenericMessage *get_front();
    GenericMessage *atomic_get_front();
    void pop_front() noexcept;
    bool is_empty() const noexcept;

    RecieveRight *atomic_get_right(u64 right_id);

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
    static Port *atomic_create_port(proc::TaskDescriptor *task) noexcept;

    /// Deletes the port. Returns false if the port is already deleted
    bool delete_self() noexcept;

    // There is no lock here because it can only be done by the owner thread, so it can only run on one CPU at a time
    u64 new_right_id() { return ++current_right_id; }

    bool atomic_alive() const;

    static ReturnStr<std::pair<Right * /* right */, u64 /* new_id_error */>>
        send_message_right(Right *right, proc::TaskGroup *verify_group, Port *reply_port,
                           rights_array array, message_buffer data, uint64_t sender_id,
                           RightType new_right_type, bool always_destroy_right);

protected:
    using Message_storage = pmos::containers::CircularDoubleList<GenericMessage, &GenericMessage::list_node>;
    Message_storage msg_queue;
    u64 current_right_id = 0;

    union {
        pmos::containers::RBTreeNode<Port> bst_head_global;
        memory::RCU_Head rcu_head;
    };
    pmos::containers::RBTreeNode<Port> bst_head_owner;

    pmos::containers::set<proc::TaskGroup *> notifier_ports;
    mutable Spinlock notifier_ports_lock;

    using global_ports_tree =
        pmos::containers::RedBlackTree<Port, &Port::bst_head_global,
                                       detail::TreeCmp<Port, u64, &Port::portno>>;

    static inline u64 biggest_port = 1;
    static inline global_ports_tree::RBTreeHead ports;
    static inline Spinlock ports_lock;

    using rights_tree =
        pmos::containers::RedBlackTree<RecieveRight, &RecieveRight::parent_head,
                                       detail::TreeCmp<RecieveRight, u64, &RecieveRight::right_parent_id>>;
    rights_tree::RBTreeHead rights;
    Spinlock rights_lock;

    friend class proc::TaskGroup;
    friend class proc::TaskDescriptor;
    friend struct RecieveRight;
    friend struct SendRight;
    friend struct SendManyRight;
    friend struct SendOnceRight;
    friend struct SendManyRightShared;
};

}