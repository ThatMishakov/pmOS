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
#include <types.hh>
#include <lib/memory.hh>
#include <messaging/messaging.hh>
#include <memory/paging.hh>
#include <sched/defs.hh>
#include <lib/string.hh>
#include <lib/set.hh>
#include <exceptions.hh>
#include "task_group.hh"
#include <interrupts/stack.hh>
#include <registers.hh>
#include <paging/arch_paging.hh>
#include <pmos/load_data.h>

#ifdef __x86_64__
#include <cpus/sse.hh>
#endif

#ifdef __riscv
#include <cpus/floating_point.hh>

struct Interrupt_Handler;
#endif

using PID = u64;

struct TaskPermissions {
};

enum class TaskStatus: u64 {
    TASK_RUNNING = 0,
    TASK_RUNNING_IN_SYSTEM = 1,
    TASK_READY,
    TASK_BLOCKED,
    TASK_UNINIT,
    TASK_SPECIAL, // Would be used by idle or system tasks
    TASK_DYING,
    TASK_DEAD
};

struct Task_Attributes {
    bool debug_syscalls = false;
};

class sched_queue;
class TaskGroup;
class Mem_Object;

class TaskDescriptor {
public:
    // Basic process stuff
    Task_Regs regs; // 200 on x86_64
    #ifdef __riscv
    u64 is_system = 0; // 0 if user space task, 1 if kernel task
    #endif
    PID pid;

    // Permissions
    TaskPermissions perm;

    // Attributes of the task
    Task_Attributes attr;

    // Messaging
    klib::set<klib::shared_ptr<Generic_Port>> owned_ports;    
    klib::weak_ptr<Generic_Port> blocked_by;
    klib::weak_ptr<Generic_Port> sender_hint;
    Spinlock messaging_lock;

    // Scheduling info
    klib::shared_ptr<TaskDescriptor> queue_next = nullptr;
    klib::shared_ptr<TaskDescriptor> queue_prev = nullptr;
    sched_queue *parent_queue = nullptr;
    TaskStatus status = TaskStatus::TASK_UNINIT;
    priority_t priority = 8;
    u32 cpu_affinity = 0;
    Spinlock sched_lock;

    klib::weak_ptr<TaskDescriptor> weak_self;

    // Paging
    klib::shared_ptr<Arch_Page_Table> page_table;
    u64 page_blocked_by = 0;

    // Task groups
    klib::set<klib::shared_ptr<TaskGroup>> task_groups;
    Spinlock task_groups_lock;

    // Creates and assigns an emty valid page table
    void create_new_page_table();

    // Registers a page table within a process, if it doesn't have any
    void register_page_table(klib::shared_ptr<Arch_Page_Table>);
    void atomic_register_page_table(klib::shared_ptr<Arch_Page_Table>);

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
        this->regs.program_counter() = entry;
    }

    // Switches to this task on the current CPU
    void switch_to();

    // Functions to be called before and after task switch
    // These function save and restore extra data (floating point and vector registers,
    // segment registers on x86, etc.) that are not stored upon the kernel entry
    // This function is architecture-dependent and is defined in arch/-specific directory
    void before_task_switch();
    void after_task_switch();

    // Checks if the task is uninited
    bool is_uninited() const;

    // Inits the task
    void init();


    enum Type {
        Normal,
        System,
        Idle,
    } type = Normal;

    enum class PrivilegeLevel {
        User,
        Kernel
    };

    u64 ret_hi = 0;
    u64 ret_lo = 0;

    #ifdef __x86_64__
    // SSE data on x86_64 CPUs (floating point, vector registers)
    SSE_Data sse_data;
    #elif defined(__riscv)
    // Floating point data on RISC-V CPUs

    /// Last state of the floating point register of the task
    /// If it is clean or disabled, task_fp_registers might not be present
    FloatingPointState fp_state = FloatingPointState::Disabled;

    /// Floating point registers of the task. The actual size depends on what is supported by the ISA (F, D or Q extensions)
    /// Since I expect most of the tasks to not use floating point, don't allocate it by default and only do so when saving
    /// registers is needed (when the task leaves the CPU state as dirty)
    klib::unique_ptr<u64[]> fp_registers = nullptr;
    u64 fcsr = 0;

    /// Last hart id that used the floating point unit
    u64 last_fp_hart_id = 0;


    // Interrupts handlers
    klib::set<Interrupt_Handler*> interrupt_handlers;
    inline bool can_be_rebound()
    {
        return interrupt_handlers.empty();
    }
    #endif

    Spinlock name_lock;
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
    inline void request_repeat_syscall() noexcept {
        regs.request_syscall_restart();
    }
    inline void pop_repeat_syscall() noexcept {
        regs.clear_syscall_restart();
    }

    /// Creates a process structure and returns its pid
    static klib::shared_ptr<TaskDescriptor> create_process(PrivilegeLevel level = PrivilegeLevel::User);

    /// Loads ELF into the task from the given memory object
    /// Returns true if the ELF was loaded successfully, false if the memory object data is not immediately available
    /// Throws on errors
    bool load_elf(klib::shared_ptr<Mem_Object> obj, klib::string name = "", const klib::vector<klib::unique_ptr<load_tag_generic>>& tags = {});

    /// Cleans up the task when detected that it is dead ("tombstoning")
    /// This function is called when the dead task has been scheduled to run and is needed to clean up CPU-specific data
    void cleanup_and_release(); 
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