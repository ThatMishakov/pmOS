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

#include "tasks.hh"
#include "kernel/errors.h"
#include <sched/sched.hh>
#include "idle.hh"
#include <sched/defs.hh>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <kernel/elf.h>
#include <pmos/tls.h>
#include <pmos/load_data.h>
#include <assert.h>


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
    n->task_id = get_new_task_id();

    // Add to the map of processes and to uninit list
    Auto_Lock_Scope l(tasks_map_lock);
    tasks_map.insert({n->task_id, n});

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
        Page_Table::Protection::Writeable | Page_Table::Protection::Readable, 
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
    return status == TaskStatus::TASK_UNINIT;
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

void TaskDescriptor::atomic_kill()
{
    klib::shared_ptr<TaskDescriptor> self = weak_self.lock();

    Auto_Lock_Scope scope_lock(sched_lock);
    const bool is_blocked = status == TaskStatus::TASK_BLOCKED;
    status = TaskStatus::TASK_DYING;
    if (is_blocked)
        unblock();

    // Let the scheduler stumble upon this task and kill it
    // This is done this way so that if a task is bound to a particular CPU, the the destructors
    // (in particular, interrupt mappings) will be called on that CPU
}

void TaskDescriptor::create_new_page_table()
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_UNINIT)
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
    if (status != TaskStatus::TASK_UNINIT)
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

bool TaskDescriptor::load_elf(klib::shared_ptr<Mem_Object> elf, klib::string name, const klib::vector<klib::unique_ptr<load_tag_generic>>& tags)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_UNINIT)
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
            
            u8 protection_mask = (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Protection::Executable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Protection::Readable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Protection::Writeable : 0;

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

            u8 protection_mask = (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Protection::Executable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Protection::Readable : 0;
               protection_mask |= (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Protection::Writeable : 0;

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

    for (size_t i = 0; i < ph_count; ++i) {
        const phreader& ph = phs[i];

        if (ph.type != PT_TLS)
            continue;

        const size_t size = sizeof(TLS_Data) + ((ph.p_filesz+7)&~7UL);
        klib::unique_ptr<u64[]> t(new u64[size/sizeof(u64)]);
        TLS_Data* tls_data = (TLS_Data*)t.get();

        tls_data->memsz = ph.p_memsz;
        tls_data->align = ph.allignment;
        tls_data->filesz = ph.p_filesz;

        r = elf->read_to_kernel(ph.p_offset, tls_data->data, ph.p_filesz);
        // Install memory region
        const u64 pa_size = (size + 0xFFF) & ~0xFFFUL;
        u64 tls_virt = table->atomic_create_normal_region(
            0,
            pa_size,
            Page_Table::Protection::Readable | Page_Table::Protection::Writeable,
            false,
            name + "_tls",
            0);

        auto r = table->atomic_copy_to_user(tls_virt, tls_data, size);
        if (!r)
            return false;
        
        regs.arg3() = tls_virt;
    }

    register_page_table(table);
    const u64 stack_top = init_stack();
    regs.program_counter() = header.program_entry;

    // Push stack descriptor structure
    u64 size = sizeof(load_tag_stack_descriptor) + sizeof(load_tag_close);
    // Add other tags
    for (auto& tag: tags) {
        size += tag->offset_to_next;
    }
    u64 current_offset = 0;

    u64 load_stack[size/8];
    load_tag_stack_descriptor *d = (load_tag_stack_descriptor*)&load_stack[current_offset/8];
    *d = {
        .header = LOAD_TAG_STACK_DESCRIPTOR_HEADER,
        .stack_top = stack_top,
        .stack_size = GB(2),
        .guard_size = 0,
        .unused0 = 0,
    };

    current_offset += sizeof(*d);
    for (auto& tag: tags) {
        auto ptr = load_stack + current_offset/8;
        current_offset += tag->offset_to_next;
        memcpy((char *)ptr, (const char *)tag.get(), tag->offset_to_next);
    }

    load_tag_close *h = (load_tag_close*)&load_stack[current_offset/8];
    *h = {
        .header = LOAD_TAG_CLOSE_HEADER,
    };

    const u64 tag_size_page = (size + 0xFFF) & ~0xFFFUL;

    const u64 pos = table->atomic_create_normal_region(
        0,
        tag_size_page,
        Page_Table::Protection::Readable | Page_Table::Protection::Writeable,
        false,
        name + "_load_tags",
        0
    );
    auto b = table->atomic_copy_to_user(pos, &(load_stack[0]), size);
    if (!b)
        return false;

    regs.arg1() = pos;
    regs.arg2() = size;

    init();

    return true;
}

void TaskDescriptor::cleanup_and_release()
{
    auto c = get_cpu_struct();

    assert(status == TaskStatus::TASK_DYING);
    assert(c->current_task.get() == this);

    #ifdef __riscv
    for (auto interr: interrupt_handlers) {
        c->int_handlers.remove_handler(interr->interrupt_number);
    }
    #endif

    find_new_process();
}

TaskDescriptor::TaskID TaskDescriptor::get_new_task_id()
{
    static TaskID next_id = 1;
    return __atomic_fetch_add(&next_id, 1, __ATOMIC_RELAXED);
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