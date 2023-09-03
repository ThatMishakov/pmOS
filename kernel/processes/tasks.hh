#pragma once
#include <types.hh>
#include <lib/memory.hh>
#include <interrupts/interrupts.hh>
#include <messaging/messaging.hh>
#include <memory/paging.hh>
#include <sched/defs.hh>
#include <cpus/sse.hh>
#include <lib/string.hh>
#include <lib/set.hh>
#include <exceptions.hh>
#include "task_group.hh"

using PID = u64;

struct TaskPermissions {
};

enum Process_Status: u64 {
    PROCESS_RUNNING = 0,
    PROCESS_RUNNING_IN_SYSTEM = 1,
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_UNINIT,
    PROCESS_SPECIAL, // Would be used by idle or system tasks
    PROCESS_DEAD
};

struct Task_Attributes {
    bool debug_syscalls = false;
};

class sched_queue;
class TaskGroup;

class TaskDescriptor {
public:
    // Basic process stuff
    Task_Regs regs; // 200
    PID pid;

    // Permissions
    TaskPermissions perm;

    // Attributes of the task
    Task_Attributes attr;

    // Messaging
    klib::set<klib::weak_ptr<Generic_Port>> owned_ports;
    klib::weak_ptr<Generic_Port> blocked_by;
    klib::weak_ptr<Generic_Port> sender_hint;
    Spinlock messaging_lock;

    // Scheduling info
    klib::shared_ptr<TaskDescriptor> queue_next = nullptr;
    klib::shared_ptr<TaskDescriptor> queue_prev = nullptr;
    sched_queue *parent_queue = nullptr;
    Process_Status status = PROCESS_UNINIT;
    priority_t priority = 8;
    u32 cpu_affinity = 0;
    Spinlock sched_lock;

    klib::weak_ptr<TaskDescriptor> weak_self;

    // Paging
    klib::shared_ptr<Page_Table> page_table;
    u64 page_blocked_by = 0;

    // Task groups
    klib::set<klib::shared_ptr<TaskGroup>> task_groups;
    Spinlock task_groups_lock;

    // Creates and assigns an emty valid page table
    void create_new_page_table();

    // Registers a page table within a process, if it doesn't have any
    void register_page_table(klib::shared_ptr<Page_Table>);

    // Inits stack
    u64 init_stack();

    // Blocks the process by a page (for example in case of a pagefault)
    void atomic_block_by_page(u64 page, sched_queue *push_to_queue);

    // Unblocks the task when the page becomes available
    bool atomic_try_unblock_by_page(u64 page);

    /// Tries to atomically erase the task from the queue. If task's parrent queue is not equal to *queue*,
    /// does nothing
    void atomic_erase_from_queue(sched_queue *queue) noexcept;

    // Kills the task
    void atomic_kill();


    // Returns 0 if there are no unblocking events pending. Otherwise returns 0.
    u64 check_unblock_immediately(u64 reason, u64 extra);

    // Checks if the process is blocked by the port and unblocks it if needed
    bool atomic_unblock_if_needed(const klib::shared_ptr<Generic_Port>& compare_blocked_by);

    // Sets the entry point to the task
    inline void set_entry_point(u64 entry)
    {
        this->regs.e.rip = entry;
    }

    // Switches to this task on the current CPU
    void switch_to();

    // Checks if the task is uninited
    bool is_uninited() const;

    // Inits the task
    void init();


    enum Type {
        Normal,
        System,
        Idle,
    } type = Normal;

    u64 ret_hi = 0;
    u64 ret_lo = 0;

    SSE_Data sse_data;

    klib::string name = "";

    klib::shared_ptr<TaskDescriptor> get_ptr()
    {
        return weak_self.lock();
    }

    static klib::shared_ptr<TaskDescriptor> create()
    {
        klib::shared_ptr<TaskDescriptor> p = klib::shared_ptr<TaskDescriptor>(new TaskDescriptor());
        p->weak_self = p;
        return p;
    }

    ~TaskDescriptor() noexcept;

    // Changes the *task* to repeat the syscall upon reentering the system
    void request_repeat_syscall();
    void pop_repeat_syscall();

    // Creates a process structure and returns its pid
    static klib::shared_ptr<TaskDescriptor> create_process(u16 ring = 3);
protected:
    TaskDescriptor() = default;

    // Unblocks the task from the blocked state
    void unblock() noexcept;
};

using task_ptr = klib::shared_ptr<TaskDescriptor>;

// Inits an idle process
void init_idle();

// A map of all the tasks
using sched_map = klib::splay_tree_map<PID, klib::weak_ptr<TaskDescriptor>>;
extern sched_map tasks_map;
extern Spinlock tasks_map_lock;

// Assigns unused PID
PID assign_pid();

// Returns true if the process with pid exists, false otherwise
inline bool exists_process(u64 pid)
{
    tasks_map_lock.lock();
    bool exists = tasks_map.count(pid) == 1;
    tasks_map_lock.unlock();
    return exists;
}

// Gets a task descriptor of the process with pid
inline klib::shared_ptr<TaskDescriptor> get_task(u64 pid)
{
    Auto_Lock_Scope scope_lock(tasks_map_lock);
    return tasks_map.get_copy_or_default(pid).lock();
}

// Gets a task descriptor of the process with pid *pid*. Throws 
inline klib::shared_ptr<TaskDescriptor> get_task_throw(u64 pid)
{
    Auto_Lock_Scope scope_lock(tasks_map_lock);

    try {
        const auto& t = tasks_map.at(pid);

        const auto ptr = t.lock();

        if (!ptr) {
            throw Kern_Exception(ERROR_NO_SUCH_PROCESS, "Requested process does not exist");
        }

        return ptr;
    } catch (const std::out_of_range&) {
        throw Kern_Exception(ERROR_NO_SUCH_PROCESS, "Requested process does not exist");
    }
}

inline TaskDescriptor::~TaskDescriptor() noexcept
{
    Auto_Lock_Scope scope_lock(tasks_map_lock);
    tasks_map.erase(pid);
}