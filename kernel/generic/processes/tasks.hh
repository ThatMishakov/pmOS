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
#include "task_group.hh"

#include <assert.h>
#include <atomic>
#include <errno.h>
#include <exceptions.hh>
#include <interrupts/stack.hh>
#include <lib/memory.hh>
#include <lib/set.hh>
#include <lib/string.hh>
#include <memory/paging.hh>
#include <memory/rcu.hh>
#include <messaging/messaging.hh>
#include <paging/arch_paging.hh>
#include <pmos/containers/set.hh>
#include <pmos/load_data.h>
#include <registers.hh>
#include <sched/defs.hh>
#include <types.hh>

#if defined(__x86_64__) || defined(__i386__)
    #include <cpus/sse.hh>
#endif

#ifdef __riscv
    #include <cpus/floating_point.hh>
#endif

namespace kernel
{

namespace sched
{
    class sched_queue;
    struct CPU_Info;
}; // namespace sched
namespace paging
{
    class Mem_Object;
};
namespace interrupts
{
    struct Interrupt_Handler;
};

namespace proc
{
    class TaskGroup;

    struct TaskPermissions {
    };

    enum class TaskStatus : u64 {
        TASK_RUNNING = 0,
        TASK_READY,
        TASK_BLOCKED,
        TASK_UNINIT,
        TASK_SPECIAL, // Would be used by idle or system tasks
        TASK_DYING,
        TASK_PAUSED,
        TASK_DEAD
    };

    struct Task_Attributes {
        bool debug_syscalls = false;
    };

    class TaskDescriptor
    {
    public:
        using TaskID = u64;

        // Basic process stuff
        Task_Regs regs; // 200 on x86_64
#if defined(__riscv) || defined(__loongarch__)
        u64 is_system = 0; // 0 if user space task, 1 if kernel task
#endif
        TaskID task_id;

        // Permissions
        TaskPermissions perm;

        // Attributes of the task
        Task_Attributes attr;

        // Messaging
        using ports_tree =
            pmos::containers::RedBlackTree<ipc::Port, &ipc::Port::bst_head_owner,
                                           detail::TreeCmp<ipc::Port, u64, &ipc::Port::portno>>;
        ports_tree::RBTreeHead owned_ports;
        ipc::Port *blocked_by;
        ipc::Port *sender_hint;

        std::atomic<TaskGroup *> rights_namespace;

        // Scheduling info
        TaskDescriptor *queue_next       = nullptr;
        TaskDescriptor *queue_prev       = nullptr;
        sched::sched_queue *parent_queue = nullptr;
        TaskStatus status                = TaskStatus::TASK_UNINIT;
        u32 sched_pending_mask           = 0;
        priority_t priority              = 8;
        u32 cpu_affinity                 = 0;
        Spinlock sched_lock;

        union {
            memory::RCU_Head rcu_head;
            pmos::containers::RBTreeNode<TaskDescriptor> task_tree_head = {};
        };

        static constexpr int SCHED_PENDING_PAUSE = 1;

        sched::sched_queue waiting_to_pause;

        // Paging
        klib::shared_ptr<paging::Arch_Page_Table> page_table;
        void *page_blocked_by = nullptr;

        // Task groups. Using sched_lock...
        klib::set<TaskGroup *> task_groups;

        // Creates and assigns an emty valid page table
        kresult_t create_new_page_table();

        // Registers a page table within a process, if it doesn't have any
        [[nodiscard]] kresult_t register_page_table(klib::shared_ptr<paging::Arch_Page_Table>);
        [[nodiscard]] kresult_t
            atomic_register_page_table(klib::shared_ptr<paging::Arch_Page_Table>);

        // Inits stack
        ReturnStr<size_t> init_stack();

        // Blocks the process by a page (for example in case of a pagefault)
        void atomic_block_by_page(void *page, sched::sched_queue *push_to_queue);

        // Unblocks the task when the page becomes available
        bool atomic_try_unblock_by_page(void *page);

        /// Tries to atomically erase the task from the queue. If task's parrent queue is not equal
        /// to *queue*, does nothing
        void atomic_erase_from_queue(sched::sched_queue *queue) noexcept;

        // Kills the task
        void atomic_kill();

        // Returns 0 if there are no unblocking events pending. Otherwise returns 0.
        u64 check_unblock_immediately(u64 reason, u64 extra);

        // Checks if the process is blocked by the port and unblocks it if needed
        bool atomic_unblock_if_needed(ipc::Port *compare_blocked_by);

        // Sets the entry point to the task
        inline void set_entry_point(u64 entry) { this->regs.program_counter() = entry; }

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

        enum class PrivilegeLevel { User, Kernel };

        u64 ret_hi = 0;
        u64 ret_lo = 0;

#if defined(__x86_64__) || defined(__i386__)
        // SSE data on x86_64 CPUs (floating point, vector registers)
        x86::sse::SSE_Data sse_data;
        bool holds_sse_data = false;
#elif defined(__riscv)
        // Floating point data on RISC-V CPUs

        /// Last state of the floating point register of the task
        /// If it is clean or disabled, task_fp_registers might not be present
        riscv64::fp::FloatingPointState fp_state = riscv64::fp::FloatingPointState::Disabled;

        /// Floating point registers of the task. The actual size depends on what is supported by
        /// the ISA (F, D or Q extensions) Since I expect most of the tasks to not use floating
        /// point, don't allocate it by default and only do so when saving registers is needed (when
        /// the task leaves the CPU state as dirty)
        klib::unique_ptr<u64[]> fp_registers = nullptr;
        u64 fcsr                             = 0;

        /// Last hart id that used the floating point unit
        u64 last_fp_hart_id = 0;
#elif defined(__loongarch__)
        klib::unique_ptr<u64[]> fp_registers = nullptr;
        bool using_fp                        = false;
#endif
        u64 syscall_num = 0;

        // Interrupts handlers
        pmos::containers::set<interrupts::Interrupt_Handler *> interrupt_handlers;
        inline bool can_be_rebound() { return interrupt_handlers.empty(); }

        // Debugging
        bool cleaned_up = false;

        Spinlock name_lock;
        klib::string name = "";

        ~TaskDescriptor() noexcept;

        // Changes the *task* to repeat the syscall upon reentering the system
        inline void request_repeat_syscall() noexcept { regs.request_syscall_restart(); }
        inline void pop_repeat_syscall() noexcept { regs.clear_syscall_restart(); }

        /// Creates a process structure and returns its pid
        static TaskDescriptor *create_process(PrivilegeLevel level = PrivilegeLevel::User) noexcept;

        /// Loads ELF into the task from the given memory object
        /// Returns true if the ELF was loaded successfully, false if the memory object data is not
        /// immediately available
        ReturnStr<bool> load_elf(klib::shared_ptr<paging::Mem_Object> obj, klib::string name = "",
                                 const klib::vector<klib::unique_ptr<load_tag_generic>> &tags = {});

        /// Cleans up the task when detected that it is dead ("tombstoning")
        /// This function is called when the dead task has been scheduled to run and is needed to
        /// clean up CPU-specific data
        void cleanup();

        /// Gets an unused task ID
        static TaskID get_new_task_id();

        struct NotifierPort {
            u64 action_mask = 0;

            static constexpr int NOTIFY_EXIT       = 1;
            static constexpr int NOTIFY_SEGFAULT   = 2;
            static constexpr int NOTIFY_BREAKPOINT = 4;
        };

        klib::splay_tree_map<u64, NotifierPort> notifier_ports;
        mutable Spinlock notifier_ports_lock;

        /**
         * @brief Changes the notifier port to the new mask. Setting mask to 0 effectively removes
         * the port from the notifiers map
         *
         * @param port Port whose mask is to be changed. Must not be nullptr.
         * @param mask New mask
         * @return u64 Old mask
         */
        u64 atomic_change_notifier_mask(ipc::Port *port, u64 mask);

        /**
         * @brief Gets the notification mask of the port. If the mask is 0, then the port is not in
         * the notifiers map.
         *
         * @param port_id ID of the port whose mask is to be checked
         * @return u64 Mas of the port
         */
        u64 atomic_get_notifier_mask(u64 port_id);

        // Returns true if the task is a kernel task
        bool is_kernel_task() const;

        // Interrupts restarting syscall, setting the return value to interrupted error
        void interrupt_restart_syscall();

        // Unblocks the task if it is not already blocked
        void atomic_try_unblock();

        bool is_32bit() const;

#if defined(__riscv) || defined(__loongarch__)
        kresult_t init_fp_state();
#endif

        inline TaskGroup *get_rights_namespace() const
        {
            return rights_namespace.load(std::memory_order::consume);
        }

    protected:
        TaskDescriptor() = default;

        // Unblocks the task from the blocked state
        void unblock() noexcept;

        kresult_t set_32bit();
    };

    using task_ptr = TaskDescriptor *;

    // Inits an idle process
    kresult_t init_idle(sched::CPU_Info *cpu);

    // A map of all the tasks
    using sched_map = pmos::containers::RedBlackTree<
        TaskDescriptor, &TaskDescriptor::task_tree_head,
        detail::TreeCmp<TaskDescriptor, u64, &TaskDescriptor::task_id>>::RBTreeHead;
    extern sched_map tasks_map;
    extern Spinlock tasks_map_lock;

    // Returns true if the process with pid exists, false otherwise
    inline bool exists_process(u64 pid)
    {
        tasks_map_lock.lock();
        auto it = tasks_map.find(pid);
        tasks_map_lock.unlock();
        return it;
    }

    // Gets a task descriptor of the process with pid
    inline TaskDescriptor *get_task(u64 pid)
    {
        Auto_Lock_Scope scope_lock(tasks_map_lock);
        return tasks_map.find(pid);
    }

}; // namespace proc
}; // namespace kernel