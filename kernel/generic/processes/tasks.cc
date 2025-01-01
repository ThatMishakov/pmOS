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

#include "idle.hh"

#include <assert.h>
#include <cstddef>
#include <errno.h>
#include <exceptions.hh>
#include <kern_logger/kern_logger.hh>
#include <kernel/elf.h>
#include <memory/mem_object.hh>
#include <pmos/load_data.h>
#include <pmos/tls.h>
#include <sched/defs.hh>
#include <sched/sched.hh>
#include "syscalls.hh"

TaskDescriptor *TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel level) noexcept
{
    // Create the structure
    TaskDescriptor *n = new TaskDescriptor();
    if (!n)
        return nullptr;

#ifdef __x86_64__
    // Assign cs and ss
    switch (level) {
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

    // Allocate FPU state
    if (level == PrivilegeLevel::User) {
        n->fp_registers =
            klib::unique_ptr<u64[]>(new u64[fp_register_size(max_supported_fp_level) * 2]);
        if (!n->fp_registers)
            return nullptr;
    }
#endif

    // Assign a pid
    n->task_id = get_new_task_id();

    // Add to the map of processes and to uninit list
    Auto_Lock_Scope l(tasks_map_lock);
    tasks_map.insert(n);

    Auto_Lock_Scope uninit_lock(uninit.lock);
    uninit.push_back(n);

    if (level == PrivilegeLevel::User) {
        auto name = get_current_task()->name.clone();
        Auto_Lock_Scope scope_lock(n->name_lock);
        n->name = klib::move(name);
    }

    return n;
}

ReturnStr<size_t> TaskDescriptor::init_stack()
{
    // TODO: Check if page table exists and that task is uninited
    assert(sched_lock.is_locked());
    if (!page_table)
        return Error(-EINVAL);

    ulong stack_size = is_32bit() ? MB(32) : GB(2);

    // Prealloc a page for the stack
    u64 stack_end        = page_table->user_addr_max(); //&_free_to_use;
    u64 stack_page_start = stack_end - stack_size;

    static const klib::string stack_region_name = "default_stack";

    auto r = this->page_table->atomic_create_normal_region(
        stack_page_start, stack_size,
        Page_Table::Protection::Writeable | Page_Table::Protection::Readable, true, false,
        stack_region_name, -1, true);

    if (!r.success())
        return r.propagate();

    // Set new rsp
    this->regs.stack_pointer() = stack_end;

    return this->regs.stack_pointer();
}

extern klib::shared_ptr<Arch_Page_Table> idle_page_table;

kresult_t init_idle(CPU_Info *cpu_str)
{
    // This would not work outside of kernel initialization
    klib::unique_ptr<TaskDescriptor> i =
        TaskDescriptor::create_process(TaskDescriptor::PrivilegeLevel::Kernel);
    if (!i) [[unlikely]]
        return -ENOMEM;

    auto result = i->atomic_register_page_table(idle_page_table);
    if (result != 0) [[unlikely]]
        return result;

    Auto_Lock_Scope lock(i->sched_lock);

    sched_queue *idle_parent_queue = i->parent_queue;
    if (idle_parent_queue != nullptr) {
        Auto_Lock_Scope q_lock(idle_parent_queue->lock);
        idle_parent_queue->erase(i.get());
    }

    cpu_str->idle_task = i.release();

    // Idle has the same stack pointer as the kernel
    // Also, one idle is created per CPU
    cpu_str->idle_task->regs.thread_pointer() = (u64)cpu_str;

    // Init stack
    cpu_str->idle_task->regs.stack_pointer() = (u64)cpu_str->idle_stack.get_stack_top();

    cpu_str->idle_task->regs.program_counter() = (u64)&idle;
    cpu_str->idle_task->type                   = TaskDescriptor::Type::Idle;

    cpu_str->idle_task->priority = idle_priority;
    cpu_str->idle_task->name     = "idle";

    return 0;
}

bool TaskDescriptor::is_uninited() const { return status == TaskStatus::TASK_UNINIT; }

void TaskDescriptor::init()
{
    assert(sched_lock.is_locked());

    // TODO: This function has a race condition
    parent_queue->atomic_erase(this);
    auto cpu_struct = get_cpu_struct();

    TaskDescriptor *current_task = cpu_struct->current_task;
    if (cpu_affinity != 0 and (cpu_affinity - 1) != cpu_struct->cpu_id) {
        auto cpu = cpu_affinity - 1;
        push_ready(this);
        assert(cpu < cpus.size());
        auto remote_cpu = cpus[cpu];
        assert(remote_cpu);
        assert(remote_cpu->cpu_id == cpu);
        assert(remote_cpu != cpu_struct);

        // TODO: Give up the lock in here
        if (remote_cpu->current_task_priority > priority)
            remote_cpu->ipi_reschedule();
    } else if (current_task->priority > priority) {
        Auto_Lock_Scope scope_l(current_task->sched_lock);

        switch_to();

        push_ready(current_task);
    } else {
        push_ready(this);
    }
}

void TaskDescriptor::atomic_kill()
{
    bool reschedule = false;

    {
        Auto_Lock_Scope scope_lock(sched_lock);
        const bool is_blocked = status == TaskStatus::TASK_BLOCKED;
        status                = TaskStatus::TASK_DYING;
        if (is_blocked)
            unblock();
        else
            reschedule = true;

        if (page_table) {
            Auto_Lock_Scope page_table_lock(page_table->lock);
            page_table->owner_tasks.erase(this);
        }
    }

    bool cont = true;
    while (cont) {
        TaskDescriptor *t;
        {
            Auto_Lock_Scope scope_lock(waiting_to_pause.lock);
            t = waiting_to_pause.front();
        }
        if (!t)
            cont = false;
        else {
            t->atomic_try_unblock();
        }
    }

    if (reschedule)
        ::reschedule();

    // Let the scheduler stumble upon this task and kill it
    // This is done this way so that if a task is bound to a particular CPU, the the destructors
    // (in particular, interrupt mappings) will be called on that CPU
}

kresult_t TaskDescriptor::create_new_page_table()
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_UNINIT)
        return -EEXIST;

    if (page_table)
        return -EEXIST;

    klib::shared_ptr<Arch_Page_Table> table = Arch_Page_Table::create_empty();
    if (!table)
        return -ENOMEM;

    Auto_Lock_Scope page_table_lock(table->lock);
    if (table->owner_tasks.insert_noexcept(this).first == table->owner_tasks.end())
        return -ENOMEM;

    page_table = table;

    return 0;
}

kresult_t TaskDescriptor::register_page_table(klib::shared_ptr<Arch_Page_Table> table)
{
    if (status != TaskStatus::TASK_UNINIT)
        return -EEXIST;

    if (page_table)
        return -EEXIST;

    if (table->is_32bit()) {
        auto result = set_32bit();
        if (result != 0)
            return result;
    }

    Auto_Lock_Scope page_table_lock(table->lock);
    if (table->owner_tasks.insert_noexcept(this).first == table->owner_tasks.end())
        return -ENOMEM;

    page_table = table;

    return 0;
}

kresult_t TaskDescriptor::atomic_register_page_table(klib::shared_ptr<Arch_Page_Table> table)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    return register_page_table(klib::forward<klib::shared_ptr<Arch_Page_Table>>(table));
}

ReturnStr<bool>
    TaskDescriptor::load_elf(klib::shared_ptr<Mem_Object> elf, klib::string name,
                             const klib::vector<klib::unique_ptr<load_tag_generic>> &tags)
{
    Auto_Lock_Scope scope_lock(sched_lock);

    if (status != TaskStatus::TASK_UNINIT)
        return Error(-EEXIST);

    if (page_table)
        return Error(-EEXIST);

    ELF_Common header;
    auto r = elf->read_to_kernel(0, (u8 *)&header, sizeof(header));
    if (!r.success())
        return r.propagate();

    if (!r.val)
        // ELF header can't be read immediately
        return false;

    if (header.magic != ELF_MAGIC)
        return Error(-ENOEXEC);

    if (header.endianness != ELF_ENDIANNESS)
        return Error(-ENOEXEC);

    if (header.type != ELF_EXEC)
        return Error(-ENOEXEC);

    int paging_flags = 0;
#ifdef __x86_64__
    if (header.instr_set != ELF_X86_64 && header.instr_set != ELF_X86)
        return Error(-ENOEXEC);

    if (header.instr_set != ELF_X86_64)
        paging_flags |= Page_Table::FLAG_32BIT;
#else
    if (header.instr_set != ELF_INSTR_SET)
        return Error(-ENOEXEC);
#endif

    klib::shared_ptr<Arch_Page_Table> table;
    ulong program_entry;
    load_tag_elf_phdr phdr_tag = {LOAD_TAG_ELF_PHDR_HEADER, 0, 0, 0, 0};
    size_t load_count          = 0;

    if (header.bitness == ELF_32BIT) {
        using phreader = ELF_PHeader_32;

        ELF_32bit header;
        auto r = elf->read_to_kernel(0, (u8 *)&header, sizeof(ELF_32bit));
        if (!r.success())
            return r.propagate();

        if (header.prog_header_size != sizeof(phreader))
            return Error(-ENOEXEC);

        const u32 ph_count = header.program_header_entries;
        klib::vector<phreader> phs;
        if (!phs.resize(ph_count))
            return Error(-ENOMEM);

        unsigned pheader_size = ph_count * sizeof(phreader);
        phdr_tag.phdr_num     = ph_count;
        phdr_tag.phdr_size    = sizeof(phreader);

        r = elf->read_to_kernel(header.program_header, (u8 *)phs.data(),
                                ph_count * sizeof(phreader));
        if (!r.success())
            return r.propagate();

        if (!r.val)
            // Program headers can't be read immediately
            return false;

        // Create a new page table
        table = Arch_Page_Table::create_empty(paging_flags);
        if (!table)
            return Error(-ENOMEM);

        ulong stack_ptr  = 0;
        ulong stack_size = 0;

        // Load the program header into the page table
        for (size_t i = 0; i < ph_count; ++i) {
            const phreader &ph = phs[i];

            if (ph.type != ELF_SEGMENT_LOAD)
                continue;

            if ((ph.p_vaddr & 0xfff) != (ph.p_offset & 0xfff))
                return Error(-ENOEXEC);

            // If pheader is inside this segment, set its virtual address
            if (ph.p_offset <= header.program_header &&
                header.program_header + pheader_size <= ph.p_offset + ph.p_filesz) {
                phdr_tag.phdr_addr = ph.p_vaddr + header.program_header - ph.p_offset;
            }

            if (!(ph.flags & ELF_FLAG_WRITABLE)) {
                // Direct map the region
                const u32 region_start = ph.p_vaddr & ~0xFFFUL;
                const u32 file_offset  = ph.p_offset & ~0xFFFUL;
                const u32 size         = ((ph.p_vaddr & 0xFFFUL) + ph.p_memsz + 0xFFF) & ~0xFFFUL;

                u8 protection_mask =
                    (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Protection::Executable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Protection::Readable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Protection::Writeable : 0;

                auto res = table->atomic_create_mem_object_region(region_start, size,
                                                                  protection_mask, true, name, elf,
                                                                  false, 0, file_offset, size);
                if (!res.success())
                    return res.propagate();
            } else {
                // Copy the region on access
                const u32 region_start = ph.p_vaddr & ~0xFFFUL;
                const u32 size =
                    ((ph.p_vaddr & (u32)0xFFF) + ph.p_memsz + (u32)0xFFF) & ~(u32)0xFFF;
                const u32 file_offset         = ph.p_offset;
                const u32 file_size           = ph.p_filesz;
                const u32 object_start_offset = ph.p_vaddr - region_start;

                u8 protection_mask =
                    (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Protection::Executable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Protection::Readable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Protection::Writeable : 0;

                auto res = table->atomic_create_mem_object_region(
                    region_start, size, protection_mask, true, name, elf, true, object_start_offset,
                    file_offset, file_size);

                if (!res.success())
                    return res.propagate();
            }
        }

        for (size_t i = 0; i < ph_count; ++i) {
            const phreader &ph = phs[i];

            if (ph.type != PT_TLS)
                continue;

            const size_t size = sizeof(TLS_Data) + ((ph.p_filesz + 7) & ~7UL);
            klib::unique_ptr<u32[]> t(new u32[size / sizeof(u32)]);
            if (!t)
                return Error(-ENOMEM);

            TLS_Data *tls_data = (TLS_Data *)t.get();

            tls_data->memsz  = ph.p_memsz;
            tls_data->align  = ph.allignment;
            tls_data->filesz = ph.p_filesz;

            if (ph.p_filesz) {
                r = elf->read_to_kernel(ph.p_offset, tls_data->data, ph.p_filesz);
                if (!r.success())
                    return r.propagate();
            }

            if (!r.val)
                return false;

            // Install memory region
            const u32 pa_size = (size + 0xFFF) & ~0xFFF;
            auto tls_virt     = table->atomic_create_normal_region(
                0, pa_size, Page_Table::Protection::Readable | Page_Table::Protection::Writeable,
                false, false, name + "_tls", 0, true);

            if (!tls_virt.success())
                return tls_virt.propagate();

            auto r = table->atomic_copy_to_user(tls_virt.val->start_addr, tls_data, size);
            if (!r.success())
                return r.propagate();

            if (!r.val)
                return false;

            regs.arg3() = tls_virt.val->start_addr;
        }

        program_entry = header.program_entry;
    } else {
        // Parse program headers
        using phreader = ELF_PHeader_64;

        ELF_64bit header;
        auto r = elf->read_to_kernel(0, (u8 *)&header, sizeof(ELF_64bit));
        if (!r.success())
            return r.propagate();

        if (header.prog_header_size != sizeof(phreader))
            return Error(-ENOEXEC);

        u64 pheader_size   = header.program_header_entries * sizeof(phreader);
        phdr_tag.phdr_num  = header.program_header_entries;
        phdr_tag.phdr_size = sizeof(phreader);

        const u64 ph_count = header.program_header_entries;
        klib::vector<phreader> phs;
        if (!phs.resize(ph_count))
            return Error(-ENOMEM);

        r = elf->read_to_kernel(header.program_header, (u8 *)phs.data(),
                                ph_count * sizeof(phreader));
        if (!r.success())
            return r.propagate();

        if (!r.val)
            // Program headers can't be read immediately
            return false;

        // Create a new page table
        table = Arch_Page_Table::create_empty(paging_flags);
        if (!table)
            return Error(-ENOMEM);

        // Load the program header into the page table
        for (size_t i = 0; i < ph_count; ++i) {
            const phreader &ph = phs[i];

            if (ph.type != ELF_SEGMENT_LOAD)
                continue;

            if ((ph.p_vaddr & 0xfff) != (ph.p_offset & 0xfff))
                return (-ENOEXEC);

            if (ph.p_offset <= header.program_header &&
                header.program_header + pheader_size <= ph.p_offset + ph.p_filesz) {
                phdr_tag.phdr_addr = ph.p_vaddr + header.program_header - ph.p_offset;
            }

            if (!(ph.flags & ELF_FLAG_WRITABLE)) {
                // Direct map the region
                const u64 region_start = ph.p_vaddr & ~0xFFFUL;
                const u64 file_offset  = ph.p_offset & ~0xFFFUL;
                const u64 size         = ((ph.p_vaddr & 0xFFFUL) + ph.p_memsz + 0xFFF) & ~0xFFFUL;

                u8 protection_mask =
                    (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Protection::Executable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Protection::Readable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Protection::Writeable : 0;

                auto res = table->atomic_create_mem_object_region(region_start, size,
                                                                  protection_mask, true, name, elf,
                                                                  false, 0, file_offset, size);
                if (!res.success())
                    return res.propagate();
            } else {
                // Copy the region on access
                const u64 region_start = ph.p_vaddr & ~0xFFFUL;
                const u64 size         = ((ph.p_vaddr & 0xFFFUL) + ph.p_memsz + 0xFFF) & ~0xFFFUL;
                const u64 file_offset  = ph.p_offset;
                const u64 file_size    = ph.p_filesz;
                const u64 object_start_offset = ph.p_vaddr - region_start;

                u8 protection_mask =
                    (ph.flags & ELF_FLAG_EXECUTABLE) ? Page_Table::Protection::Executable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_READABLE) ? Page_Table::Protection::Readable : 0;
                protection_mask |=
                    (ph.flags & ELF_FLAG_WRITABLE) ? Page_Table::Protection::Writeable : 0;

                auto res = table->atomic_create_mem_object_region(
                    region_start, size, protection_mask, true, name, elf, true, object_start_offset,
                    file_offset, file_size);

                if (!res.success())
                    return res.propagate();
            }
        }

        for (size_t i = 0; i < ph_count; ++i) {
            const phreader &ph = phs[i];

            if (ph.type != PT_TLS)
                continue;

            const size_t size = sizeof(TLS_Data) + ((ph.p_filesz + 7) & ~7UL);
            klib::unique_ptr<u64[]> t(new u64[size / sizeof(u64)]);
            if (!t)
                return Error(-ENOMEM);

            TLS_Data *tls_data = (TLS_Data *)t.get();

            tls_data->memsz  = ph.p_memsz;
            tls_data->align  = ph.allignment;
            tls_data->filesz = ph.p_filesz;

            r = elf->read_to_kernel(ph.p_offset, tls_data->data, ph.p_filesz);
            if (!r.success())
                return r.propagate();

            if (!r.val)
                return false;

            // Install memory region
            const u64 pa_size = (size + 0xFFF) & ~0xFFFUL;
            auto tls_virt     = table->atomic_create_normal_region(
                0, pa_size, Page_Table::Protection::Readable | Page_Table::Protection::Writeable,
                false, false, name + "_tls", 0, true);

            if (!tls_virt.success())
                return tls_virt.propagate();

            auto r = table->atomic_copy_to_user(tls_virt.val->start_addr, tls_data, size);
            if (!r.success())
                return r.propagate();

            if (!r.val)
                return false;

            regs.arg3() = tls_virt.val->start_addr;
        }

        program_entry = header.program_entry;
    }

    if (phdr_tag.phdr_addr == 0) {
        serial_logger.printf("load_elf error -> phdr not mapped into the program's memory\n");
        return Error(-ENOEXEC);
    }

    auto result = register_page_table(table);
    if (result != 0)
        return Error(result);

    auto stack_top = init_stack();

    if (!stack_top.success())
        return stack_top.propagate();

    regs.program_counter() = program_entry;

    // Push stack descriptor structure
    u64 size =
        sizeof(load_tag_stack_descriptor) + sizeof(load_tag_elf_phdr) + sizeof(load_tag_close);
    // Add other tags
    for (auto &tag: tags) {
        size += tag->offset_to_next;
    }
    u64 current_offset = 0;

    u64 load_stack[size / 8];
    load_tag_stack_descriptor *d = (load_tag_stack_descriptor *)&load_stack[current_offset / 8];
    *d                           = {
                                  .header     = LOAD_TAG_STACK_DESCRIPTOR_HEADER,
                                  .stack_top  = stack_top.val,
                                  .stack_size = is_32bit() ? MB(32) : GB(2),
                                  .guard_size = 0,
                                  .unused0    = 0,
    };

    current_offset += sizeof(*d);

    memcpy((char *)(load_stack) + current_offset, (char *)&phdr_tag, sizeof(phdr_tag));
    current_offset += sizeof(phdr_tag);

    for (auto &tag: tags) {
        auto ptr = load_stack + current_offset / 8;
        current_offset += tag->offset_to_next;
        memcpy((char *)ptr, (const char *)tag.get(), tag->offset_to_next);
    }

    load_tag_close *h = (load_tag_close *)&load_stack[current_offset / 8];
    *h                = {
                       .header = LOAD_TAG_CLOSE_HEADER,
    };

    const u64 tag_size_page = (size + 0xFFF) & ~0xFFFUL;

    auto pos_p = table->atomic_create_normal_region(
        0, tag_size_page, Page_Table::Protection::Readable | Page_Table::Protection::Writeable,
        false, false, name + "_load_tags", 0, true);
    if (!pos_p.success())
        return pos_p.propagate();

    auto pos = pos_p.val->start_addr;

    auto b = table->atomic_copy_to_user(pos, &(load_stack[0]), size);
    if (!b.success())
        return b.propagate();

    if (!b.val)
        return false;

    regs.arg1() = pos;
    regs.arg2() = size;

    init();

    return true;
}

void TaskDescriptor::cleanup()
{
    cleaned_up = true;

    {
        Auto_Lock_Scope scope_lock(tasks_map_lock);
        tasks_map.erase(this);
    }

    auto c = get_cpu_struct();
    for (auto &interr: interrupt_handlers) {
        c->int_handlers.remove_handler(interr->interrupt_number);
    }

    auto get_first_port = [&]() -> Port * {
        Auto_Lock_Scope scope_lock(sched_lock);
        auto it = owned_ports.begin();
        return it == owned_ports.end() ? nullptr : &*it;
    };

    Port *port;
    while ((port = get_first_port())) {
        port->delete_self();
    }

    rcu_head.rcu_func = [](void *self, bool) {
        TaskDescriptor *t = reinterpret_cast<TaskDescriptor *>(reinterpret_cast<char *>(self) -
                                                               offsetof(TaskDescriptor, rcu_head));
        delete t;
    };
    get_cpu_struct()->heap_rcu_cpu.push(&rcu_head);
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

TaskDescriptor::~TaskDescriptor() noexcept
{
    assert(status == TaskStatus::TASK_UNINIT or (status == TaskStatus::TASK_DYING and cleaned_up));

    if (status == TaskStatus::TASK_UNINIT) {
        Auto_Lock_Scope scope_lock(tasks_map_lock);
        tasks_map.erase(this);
    }
}

void TaskDescriptor::interrupt_restart_syscall()
{
    pop_repeat_syscall();
    syscall_error(this) = -EINTR;
}