#pragma once

#include <memory/rcu.hh>
#include <pmos/containers/intrusive_bst.hh>
#include <pmos/containers/intrusive_list.hh>
#include <types.hh>
#include <lib/memory.hh>
#include "messaging.hh"

namespace kernel::proc
{
class TaskGroup;
}

namespace kernel::paging
{
    class Mem_Object;
}

namespace kernel::ipc
{

struct Message;
class Port;

enum class RightType : u8 {
    SendOnce,
    SendMany,
    MemObject,
};

struct Right {
    union {
        Message *parent_message = nullptr;
        proc::TaskGroup *parent_group;
    };

    pmos::containers::RBTreeNode<Right> task_group_head;

    /// Sender-facing id (gets changed when the right is copied/moved, depending on the task group)
    u64 right_sender_id = 0;

    bool alive : 1      = true;
    bool of_message : 1 = false;
    bool right_0 : 1    = false;
    // Define this bit here to save space
    bool sent : 1              = false;
    bool notification_port : 1 = false;

    mutable Spinlock lock;

    enum class DestroyReason {
        DeletedBySender,
        DeletedByReceiver,
        SendingMessage,
    };

    bool destroy(DestroyReason reason, proc::TaskGroup *match_group = nullptr);
    virtual bool destroy_nolock(DestroyReason reason, proc::TaskGroup *match_group = nullptr);

    // Here, the reason would be DeletedBySender
    void destroy_deleting_message();

    virtual void rcu_push() = 0;

    bool of_group(proc::TaskGroup *) const;
    bool atomic_alive() const;

    ReturnStr<u64> atomic_transfer_to_group(proc::TaskGroup *from, proc::TaskGroup *to);
    // Atomically creates and returns right and its id in sender
    virtual ReturnStr<std::pair<Right *, u64>> duplicate(proc::TaskGroup *) = 0;

    virtual RightType type() const = 0;
    virtual ~Right() = default;
    virtual void remove_from_parent() = 0;

    unsigned type_as_int() const;
};

struct RecieveRight: GenericMessage {
    union {
        pmos::containers::RBTreeNode<RecieveRight> parent_head = {};
        memory::RCU_Head rcu_head;
    };

    // Parent port
    Port *parent = nullptr;

    /// Parent-facing id (does not change, and gets copied when right is duplicated)
    u64 right_parent_id = 0;

    virtual bool destroy_recieve_right() = 0;
    virtual ~RecieveRight() = default;

    // GenericMessage overrides
    virtual size_t size() const override;
    virtual ReturnStr<bool> copy_to_user_buff(char *buff) const override;
    virtual u64 sent_with_right() const override;
    virtual u64 sender_task_id() const override;
};

struct SendRight: Right, RecieveRight {
    virtual void rcu_push() override;
    virtual void remove_from_parent() override;

    // RecieveRight override
    virtual bool destroy_recieve_right() override;

    virtual Port *parent_port() = 0;
};

struct SendManyRightShared;

struct SendManyRight final: SendRight {
    static ReturnStr<SendManyRight *> create_for_group(Port *port, proc::TaskGroup *group, u64 id_in_parent);

    virtual ReturnStr<std::pair<Right *, u64>> duplicate(proc::TaskGroup *) override;
    virtual RightType type() const override;

    virtual bool destroy_nolock(DestroyReason reason, proc::TaskGroup *match_group = nullptr) override;

    pmos::containers::DoubleListHead<SendManyRight> send_many_node;
    SendManyRightShared *shared = nullptr;

    // GenericMessage overrides
    virtual void delete_self() override;

    virtual Port *parent_port() override;

    static std::pair<klib::unique_ptr<SendRight>, klib::unique_ptr<RecieveRight>> create_for_message();

};

struct SendOnceRight final: SendRight {
    static ReturnStr<SendOnceRight *> create_for_group(Port *port, proc::TaskGroup *group, u64 id_in_parent);

    virtual ReturnStr<std::pair<Right *, u64>> duplicate(proc::TaskGroup *) override;
    virtual RightType type() const override;

    // Right overrides
    virtual bool destroy_nolock(DestroyReason reason, proc::TaskGroup *match_group = nullptr) override;
    virtual void delete_self() override;

    virtual Port *parent_port() override;

    static klib::unique_ptr<SendRight> create_for_message();
};

struct SendManyRightShared final: RecieveRight {
    Spinlock lock;
    bool alive: 1 = true;
    using list = pmos::containers::CircularDoubleList<SendManyRight, &SendManyRight::send_many_node>;
    list send_many_rights;

    void rcu_push();

    virtual bool destroy_recieve_right() override;

    // GenericMessage overrides
    virtual void delete_self() override;
};

struct MemObjectRight final: Right {
    union {
        pmos::containers::DoubleListHead<MemObjectRight> parent_node = {};
        memory::RCU_Head rcu_head;
    };

    klib::shared_ptr<paging::Mem_Object> mem_object;

    static constexpr u32 PERM_READ   = 1 << 0;
    static constexpr u32 PERM_WRITE  = 1 << 1;
    static constexpr u32 PERM_DELETE = 1 << 2;
    static constexpr u32 PERM_PAGE   = 1 << 3;
    static constexpr u32 PERM_EXPAND = 1 << 4;

    static constexpr u32 PERM_ALL = (1 << 5) - 1;

    u32 permission_mask = 0;

    virtual ReturnStr<std::pair<Right *, u64>> duplicate(proc::TaskGroup *) override;

    virtual RightType type() const override;
    virtual void rcu_push() override;
    virtual void remove_from_parent() override;

    static ReturnStr<MemObjectRight *> create_for_group(klib::shared_ptr<paging::Mem_Object> mem_object,
                                            proc::TaskGroup *group, u32 permissions = PERM_ALL);
};

// Returns nullptr if the right is not a send right
SendRight *to_send_right(Right *r);

u64 atomic_right0_id();
kresult_t set_right0(Right *right, proc::TaskGroup *right_parent);

} // namespace kernel::ipc