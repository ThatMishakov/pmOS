#include "tasks.hh"
#include "kernel/errors.h"
#include <sched/sched.hh>
#include "idle.hh"
#include <sched/defs.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <kernel/elf.h>


klib::shared_ptr<TaskDescriptor> TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel level)
{
    // Create the structure
    klib::shared_ptr<TaskDescriptor> n = TaskDescriptor::create();
    
    #ifdef __x86_64__
    // Assign cs and ss
    switch (level)
    {
    case PrivilegeLevel::Kernel:
        n->regs.e.cs = R0_CODE_SEGMENT;
        n->regs.e.ss = R0_DATA_SEGMENT;
        break;
    case PrivilegeLevel::User:
        n->regs.e.cs = R3_CODE_SEGMENT;
        n->regs.e.ss = R3_DATA_SEGMENT;
        break;
    }
    #elif defined(__riscv)
    n->is_system = level == PrivilegeLevel::Kernel;
    #endif

    // Assign a pid
    n->pid = assign_pid();

    // Add to the map of processes and to uninit list
    Auto_Lock_Scope l(tasks_map_lock);
    tasks_map.insert({n->pid, n});

    Auto_Lock_Scope uninit_lock(uninit.lock);
    uninit.push_back(n);

    return n;
}

u64 TaskDescriptor::init_stack()
{
    // TODO: Check if page table exists and that task is uninited


    // Prealloc a page for the stack
    u64 stack_end = page_table->user_addr_max(); //&_free_to_use;
    u64 stack_size = GB(2);
    u64 stack_page_start = stack_end - stack_size;

    static const klib::string stack_region_name = "default_stack";

    this->page_table->atomic_create_normal_region(
        stack_page_start, 
        stack_size, 
        Page_Table::Writeable | Page_Table::Readable, 
        true,
        stack_region_name, 
        -1);

    // Set new rsp
    this->regs.stack_pointer() = stack_end;

    return this->regs.stack_pointer();
}

extern klib::shared_ptr<Arch_Page_Table> idle_page_table;

void init_idle()
{
    try {
        klib::shared_ptr<TaskDescriptor> i = TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::Kernel);
        i->atomic_register_page_table(idle_page_table);

        Auto_Lock_Scope lock(i->sched_lock);

        sched_queue* idle_parent_queue = i->parent_queue;
        if (idle_parent_queue != nullptr) {
            Auto_Lock_Scope q_lock(idle_parent_queue->lock);

            idle_parent_queue->erase(i);
        }


        CPU_Info* cpu_str = get_cpu_struct();
        cpu_str->idle_task = i;

        // Idle has the same stack pointer as the kernel
        // Also, one idle is created per CPU
        i->regs.thread_pointer() = (u64)cpu_str;

        // Init stack
        i->regs.stack_pointer() = (u64)cpu_str->idle_stack.get_stack_top();

        i->regs.program_counter() = (u64)&idle;
        i->type = TaskDescriptor::Type::Idle;

        i->priority = idle_priority;
        i->name = "idle";
    } catch (const Kern_Exception& e) {
        t_print_bochs("Error creating idle process: %i (%s)\n", e.err_code, e.err_message);
        throw e;
    }
}

bool TaskDescriptor::is_uninited() const
{
    return status == PROCESS_UNINIT;
}

void TaskDescriptor::init()
{
    klib::shared_ptr<TaskDescriptor> task = weak_self.lock();
    task->parent_queue->atomic_erase(task);

    klib::shared_ptr<TaskDescriptor> current_task = get_cpu_struct()->current_task;
    if (current_task->priority > task->priority) {
        Auto_Lock_Scope scope_l(current_task->sched_lock);

        task->switch_to();

        push_ready(current_task);
    } else {
        push_ready(task);
    }
}

void TaskDescriptor::atomic_kill() // TODO: UNIX Signals
{
    klib::shared_ptr<TaskDescriptor> self = weak_self.lock();

    Auto_Lock_Scope scope_lock(sched_lock);

    sched_queue* task_queue = parent_queue;
    if (task_queue != nullptr) {
        Auto_Lock_Scope queue_lock(task_queue->lock);

        task_queue->erase(self);
        parent_queue = nullptr;
    }

    // Task switch if it's a current process
    CPU_Info* cpu_str = get_cpu_struct();
    if (cpu_str->current_task == self) {
        find_new_process();
    }

    status = PROCESS_DEAD;
    parent_queue = NULL;
}

void TaskDescriptor::create_new_page_table()
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_UNINIT)
        throw Kern_Exception(ERROR_PROCESS_INITED, "Process is already inited");

    if (page_table)
        throw Kern_Exception(ERROR_HAS_PAGE_TABLE, "Process already has a page table");

    klib::shared_ptr<Arch_Page_Table> table = Arch_Page_Table::create_empty();

    Auto_Lock_Scope page_table_lock(table->lock);
    table->owner_tasks.insert(weak_self);
    page_table = table;
}

void TaskDescriptor::register_page_table(klib::shared_ptr<Arch_Page_Table> table)
{
    if (status != PROCESS_UNINIT)
        throw Kern_Exception(ERROR_PROCESS_INITED, "Process is already inited");

    if (page_table)
        throw Kern_Exception(ERROR_HAS_PAGE_TABLE, "Process already has a page table");

    Auto_Lock_Scope page_table_lock(table->lock);
    table->owner_tasks.insert(weak_self);
    page_table = table;
}

void TaskDescriptor::atomic_register_page_table(klib::shared_ptr<Arch_Page_Table> table)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    register_page_table(klib::forward<klib::shared_ptr<Arch_Page_Table>>(table));
}

bool TaskDescriptor::load_elf(klib::shared_ptr<Mem_Object> elf, klib::string name)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != PROCESS_UNINIT)
        throw Kern_Exception(ERROR_PROCESS_INITED, "Process is already inited");

    if (page_table)
        throw Kern_Exception(ERROR_NOT_IMPLEMENTED, "Process has a page table");

    ELF_64bit header;
    bool r = elf->read_to_kernel(0, (u8*)&header, sizeof(ELF_64bit));
    if (!r)
        // ELF header can't be read immediately
        return false;

    if (header.magic != ELF_MAGIC)
        throw Kern_Exception(ERROR_BAD_FORMAT, "Not an ELF file");

    if (header.bitness != ELF_BITNESS)
        throw Kern_Exception(ERROR_BAD_FORMAT, "Not a 64-bit ELF file");

    if (header.endianness != ELF_ENDIANNESS)
        throw Kern_Exception(ERROR_BAD_FORMAT, "Wrong endianness ELF file");

    if (header.type != ELF_EXEC)
        throw Kern_Exception(ERROR_BAD_FORMAT, "Not an executable ELF file");

    if (header.instr_set != ELF_INSTR_SET)
        throw Kern_Exception(ERROR_BAD_FORMAT, "Wrong instruction set ELF file");

    
    // Parse program headers
    using phreader = ELF_PHeader_64;
    if (header.prog_header_size != sizeof(phreader))
        throw Kern_Exception(ERROR_BAD_FORMAT, "Wrong program header size");

    const u64 ph_count = header.program_header_entries;
    klib::vector<phreader> phs(ph_count);
    r = elf->read_to_kernel(header.program_header, (u8*)phs.data(), ph_count * sizeof(phreader));
    if (!r)
        // Program headers can't be read immediately
        return false;

    // Create a new page table
    auto table = Arch_Page_Table::create_empty();

    // Load the program header into the page table
    for (size_t i = 0; i < ph_count; ++i) {
        const phreader& ph = phs[i];

        if (ph.type != ELF_SEGMENT_LOAD)
            continue;

        if ((ph.p_vaddr&0xfff) != (ph.p_offset&0xfff))
            throw Kern_Exception(ERROR_BAD_FORMAT, "Unaligned segment");

        if (!(ph.flags & ELF_FLAG_WRITABLE)) {
            // Direct map the region
            const u64 region_start = ph.p_vaddr & ~0xFFFUL;
            const u64 file_offset = ph.p_offset & ~0xFFFUL;
            const u64 size = (ph.p_memsz + 0xFFF) & ~0xFFFUL;
            
            u8 protection_mask = (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Executable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Readable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Writeable : 0;

            table->atomic_create_mem_object_region(
                region_start, 
                size, 
                protection_mask, 
                true, 
                name, 
                elf,
                false,
                0,
                file_offset,
                size
            );
        } else {
            // Copy the region on access
            const u64 region_start = ph.p_vaddr & ~0xFFFUL;
            const u64 size = (ph.p_memsz + 0xFFF) & ~0xFFFUL;
            const u64 file_offset = ph.p_offset;
            const u64 file_size = ph.p_filesz;
            const u64 object_start_offset = ph.p_vaddr - region_start;

            u8 protection_mask = (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Executable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Readable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Writeable : 0;

            table->atomic_create_mem_object_region(
                region_start, 
                size, 
                protection_mask, 
                true, 
                name, 
                elf,
                true,
                object_start_offset,
                file_offset,
                file_size
            );
        }
    }

    // Pass TLS data
    // TODO!

    register_page_table(table);
    init_stack();
    regs.program_counter() = header.program_entry;
    init();
    return true;
}

// TODO: Arch-specific!!!
// void TaskDescriptor::request_repeat_syscall()
// {
//     if (regs.entry_type != 3) {
//         regs.saved_entry_type = regs.entry_type;
//         regs.entry_type = 3;
//     }
// }

// void TaskDescriptor::pop_repeat_syscall()
// {
//     if (regs.entry_type == 3) {
//         regs.entry_type = regs.saved_entry_type;
//     }
// }