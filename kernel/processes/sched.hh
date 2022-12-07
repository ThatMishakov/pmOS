#pragma once
#include <interrupts/interrupts.hh>
#include <lib/vector.hh>
#include <messaging/messaging.hh>
#include <lib/splay_tree_map.hh>
#include <types.hh>
#include <lib/stack.hh>
#include <lib/array.hh>
#include <lib/memory.hh>

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

struct sched_pqueue;

class generic_tqueue_iterator {
public:
    virtual void atomic_erase_from_parrent() noexcept = 0;
    virtual ~generic_tqueue_iterator() = default;
};

struct TaskDescriptor {
    // Basic process stuff
    Task_Regs regs;
    PID pid;
    u64 page_table; // 192

    Spinlock page_table_lock;

    TaskPermissions perm;

    // Scheduling lists
    klib::unique_ptr<generic_tqueue_iterator> queue_iterator;
    volatile Process_Status status;
    volatile Process_Status next_status;
    Spinlock sched_lock;

    // Messaging
    Message_storage messages;
    Ports_storage ports;
    Spinlock messaging_lock;

    u64 unblock_mask = ~0;

    // Return state
    u64 ret_hi;
    u64 ret_lo;

    // Arguments
    klib::vector<u8> args;

    // Inits stack
    ReturnStr<u64> init_stack();

    // Returns 0 if there are no unblocking events pending. Otherwise returns 0.
    u64 check_unblock_immediately();

    // Switches to this process
    void switch_to();

    // Sets the entry point to the task
    inline void set_entry_point(u64 entry)
    {
        this->regs.e.rip = entry;
    }

    // Attributes of the task
    Task_Attributes attr;
    u32 quantum_ticks = 0; // 0 - infinite

    enum Type {
        Normal,
        System,
        Idle,
    } type = Normal;

    u8 priority = 1; // 3 - background
    constexpr static u8 background_priority = 3;
};

// Checks the mask and unblocks the task if needed
void unblock_if_needed(const klib::shared_ptr<TaskDescriptor>& p, u64 reason);

// Blocks task with a mask *mask*
ReturnStr<u64> block_task(const klib::shared_ptr<TaskDescriptor>& task, u64 mask);

struct sched_pqueue {
    klib::list<klib::weak_ptr<TaskDescriptor>> queue_list;
    mutable Spinlock lock;

    // Erases the task from other queue (if in any) and pushes it accordingly
    void atomic_auto_push_back(const klib::shared_ptr<TaskDescriptor>&);
    void atomic_auto_push_front(const klib::shared_ptr<TaskDescriptor>&);

    bool empty() const;

    // Returns true if queue was not empty and weak_ptr to task
    klib::pair<bool, klib::weak_ptr<TaskDescriptor>> pop_front();

    class iterator: public generic_tqueue_iterator {
    public:
        virtual void atomic_erase_from_parrent() noexcept override;

        iterator() = default;
        iterator(const iterator&) = default;
        iterator(iterator&&) = default;
        iterator(const klib::list<klib::weak_ptr<TaskDescriptor>>::iterator& l, sched_pqueue* p): list_it(l), parent(p) {};
        ~iterator() = default;
    private:
        klib::list<klib::weak_ptr<TaskDescriptor>>::iterator list_it;
        sched_pqueue* parent = nullptr;

        friend sched_pqueue;
    };
};

struct Ready_Queues {
    klib::array<sched_pqueue, 4> queues;
    static constexpr klib::array<u32, 4> quantums = {20, 40, 80, 0};
    void push_ready(const klib::shared_ptr<TaskDescriptor>& p);
    bool empty() const;

    klib::pair<bool, klib::shared_ptr<TaskDescriptor>> atomic_get_pop_first();
    static void assign_quantum_on_priority(TaskDescriptor*);

    sched_pqueue temp_ready;
};

struct Per_CPU_Scheduler {
    Ready_Queues queues;
    u32 timer_val = 0;
};

struct CPU_Info {
    CPU_Info* self = this;
    Stack* kernel_stack = nullptr;
    u64 task_switch = 0;
    klib::shared_ptr<TaskDescriptor> current_task = klib::shared_ptr<TaskDescriptor>();
    klib::shared_ptr<TaskDescriptor> next_task = klib::shared_ptr<TaskDescriptor>();
    u64 release_old_cr3 = 0;
    Per_CPU_Scheduler sched;
    klib::shared_ptr<TaskDescriptor> idle_task = klib::shared_ptr<TaskDescriptor>();
};

// static CPU_Info* const GSRELATIVE per_cpu = 0; // clang ignores GSRELATIVE for no apparent reason
extern "C" CPU_Info* get_cpu_struct();

// Adds the task to the appropriate ready queue
void push_ready(const klib::shared_ptr<TaskDescriptor>& p);

// Initializes scheduling structures during the kernel initialization
void init_scheduling();

// Assigns unused PID
PID assign_pid();

using sched_map = klib::splay_tree_map<PID, klib::shared_ptr<TaskDescriptor>>;
extern sched_map tasks_map;
extern Spinlock tasks_map_lock;

extern sched_pqueue blocked;
extern Spinlock blocked_s;

// Creates a process structure and returns its pid
ReturnStr<klib::shared_ptr<TaskDescriptor>> create_process(u16 ring = 3);

// Inits an idle process
void init_idle();

// Finds a ready process and switches to it
void task_switch();

// Finds and switches to a new process (e.g. if the current is blocked or executing for too long)
void find_new_process();

// Returns true if the process with pid exists, false otherwise
inline bool exists_process(u64 pid)
{
    tasks_map_lock.lock();
    bool exists = tasks_map.count(pid) == 1;
    tasks_map_lock.unlock();
    return exists;
}

// Returns true if the process with pid is uninitialized
bool is_uninited(const klib::shared_ptr<const TaskDescriptor>&);

// Gets a task descriptor of the process with pid
inline klib::shared_ptr<TaskDescriptor> get_task(u64 pid)
{
    tasks_map_lock.lock();
    klib::shared_ptr<TaskDescriptor> t = tasks_map.at(pid);
    tasks_map_lock.unlock();
    return t; 
}

// Kills the task
void kill(const klib::shared_ptr<TaskDescriptor>& t);

// Inits the task
void init_task(const klib::shared_ptr<TaskDescriptor>& task);

// To be called from the clock routine
void sched_periodic();

// Starts the timer and updates CPU_Info
void start_timer(u32 ms);

// Starts the timer with specified ticks
void start_timer_ticks(u32 ticks);

// Starts the scheduler
void start_scheduler();

// Pushes current processos to the back of sheduling queues and finds a new one to execute
void evict(const klib::shared_ptr<TaskDescriptor>&);