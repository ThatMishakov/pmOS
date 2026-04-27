#pragma once
#include <lib/memory.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>
#include <messaging/ports.hh>
#include <messaging/rights.hh>
#include <utility>
#include <pmos/containers/intrusive_list.hh>

namespace kernel
{

namespace sched
{
    struct CPU_Info;
}

namespace interrupts
{

struct InterruptHandler;

struct IntNotificationRight final: ipc::RecieveRight {
    // A bit of a thinking process here: initially, I wanted to make this an
    // ipc::Right, so that this can be accessed through the same APIs as other rights,
    // but I think having this just be a RecieveRight makes more sense, so that
    // other threads or whomever have no way to access this erroneously, which saves
    // the kernel from doing a bunch of checks...
    // (also, perhaps recieve rights should be rethought, and made be per-task and not per-port)

    // Also, RecieveRight being a message already (which is a bit of a questionable descision
    // in itself, but whatever) is what's wanted here already anyway

    virtual bool destroy_recieve_right() override;
    InterruptHandler *parent_handler = nullptr;

    bool alive : 1 = true;
    bool sent : 1 = false;
    bool pending_completion : 1 = false;

    pmos::containers::DoubleListHead<IntNotificationRight> notification_node;

    // GenericMessage overrides
    virtual size_t size() const override;
    virtual ReturnStr<bool> copy_to_user_buff(char *buff) const override;
    virtual void delete_self() override;
    virtual ipc::RightType recieve_type() const override;

    static ReturnStr<IntNotificationRight *> create_for_port(InterruptHandler *handler, ipc::Port *port);

    kresult_t complete();
};

struct IntSourceRight final: ipc::Right {
    union {
        pmos::containers::DoubleListHead<IntSourceRight> source_node;
        memory::RCU_Head rcu_head;
    };
    InterruptHandler *parent_handler = nullptr;

    virtual void rcu_push() override;

    virtual ReturnStr<std::pair<ipc::Right *, u64>> duplicate(proc::TaskGroup *) override;
    virtual ipc::RightType type() const override;

    virtual void remove_from_parent() override;

    static ReturnStr<IntSourceRight *> create_for_group(InterruptHandler *handler, proc::TaskGroup *group);
};

enum class NotificationResult {
    NoHandlers,
    Success,
};

struct InterruptHandler {
    pmos::containers::CircularDoubleList<IntNotificationRight, &IntNotificationRight::notification_node> notification_rights;

    // Lock is needed for source rights but not notification rights, since the latter ones are bound
    // to the parent_cpu, and thus can't be modified concurrently.
    Spinlock sources_lock;
    pmos::containers::CircularDoubleList<IntSourceRight, &IntSourceRight::source_node> sources;

    // Don't keep refcount here (or shared_ptr stuff), since the userspace is expected to
    // allocate/deallocate interrupts explicitly...

    sched::CPU_Info *parent_cpu = nullptr;

    NotificationResult send_interrupt_notification();
};



// ================ Arch-specific stuff ================

// These 3 will be called by the parent_cpu. The arch is expected to override InterruptHandler
// internally, and static upcast it to find the appropriate interrupt controller, etc.
void interrupt_enable(InterruptHandler *handler);
void interrupt_disable(InterruptHandler *handler);
void interrupt_complete(InterruptHandler *handler);

// On x86, userspace can figure out ISA -> GSI mapping itself...
ReturnStr<InterruptHandler *> allocate_or_get_handler(u32 gsi, bool edge_triggered = false, bool active_low = false);

}; // namespace interrupts
}; // namespace kernel