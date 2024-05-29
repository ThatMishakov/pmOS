/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "interrupts.hh"

#include "plic.hh"

#include <assert.h>
#include <cpus/csr.hh>
#include <cpus/floating_point.hh>
#include <kern_logger/kern_logger.hh>
#include <pmos/ipc.h>
#include <sched/sched.hh>
#include <utils.hh>

extern "C" void handle_interrupt();

struct fp_s {
    u64 fp;
    u64 ra;
};

u64 get_fp()
{
    u64 fp;
    asm volatile("add %0, x0, fp" : "=r"(fp));
    return fp;
}

void print_stack_trace_fp(u64 fp = get_fp())
{
    fp_s *current = (fp_s *)((char *)fp - 16);
    serial_logger.printf("Stack trace:\n");
    while (1) {
        serial_logger.printf("  0x%x fp 0x%x\n", current->ra, current->fp);
        if (current->fp == 0 or ((i64)current->fp > 0)) {
            break;
        }
        current = (fp_s *)current->fp - 1;
    }
}

extern "C" void print_stack_trace() { print_stack_trace_fp(); }

void print_stack_trace(Logger &logger) { print_stack_trace_fp(); }

void print_registers(const klib::shared_ptr<TaskDescriptor> &task, Logger &logger)
{
    logger.printf("Task %i registers:\n", task->task_id);
    logger.printf("  ra 0x%x\n", task->regs.ra);
    logger.printf("  sp 0x%x\n", task->regs.sp);
    logger.printf("  gp 0x%x\n", task->regs.gp);
    logger.printf("  tp 0x%x\n", task->regs.tp);
    logger.printf("  t0 0x%x\n", task->regs.t0);
    logger.printf("  t1 0x%x\n", task->regs.t1);
    logger.printf("  t2 0x%x\n", task->regs.t2);
    logger.printf("  s0 0x%x\n", task->regs.s0);
    logger.printf("  s1 0x%x\n", task->regs.s1);
    logger.printf("  a0 0x%x\n", task->regs.a0);
    logger.printf("  a1 0x%x\n", task->regs.a1);
    logger.printf("  a2 0x%x\n", task->regs.a2);
    logger.printf("  a3 0x%x\n", task->regs.a3);
    logger.printf("  a4 0x%x\n", task->regs.a4);
    logger.printf("  a5 0x%x\n", task->regs.a5);
    logger.printf("  a6 0x%x\n", task->regs.a6);
    logger.printf("  a7 0x%x\n", task->regs.a7);
    logger.printf("  s2 0x%x\n", task->regs.s2);
    logger.printf("  s3 0x%x\n", task->regs.s3);
    logger.printf("  s4 0x%x\n", task->regs.s4);
    logger.printf("  s5 0x%x\n", task->regs.s5);
    logger.printf("  s6 0x%x\n", task->regs.s6);
    logger.printf("  s7 0x%x\n", task->regs.s7);
    logger.printf("  s8 0x%x\n", task->regs.s8);
    logger.printf("  s9 0x%x\n", task->regs.s9);
    logger.printf("  s10 0x%x\n", task->regs.s10);
    logger.printf("  s11 0x%x\n", task->regs.s11);
    logger.printf("  t3 0x%x\n", task->regs.t3);
    logger.printf("  t4 0x%x\n", task->regs.t4);
    logger.printf("  t5 0x%x\n", task->regs.t5);
    logger.printf("  t6 0x%x\n", task->regs.t6);
    logger.printf("  pc 0x%x\n", task->regs.pc);
}

void page_fault(u64 virt_addr, u64 scause)
{
    const u64 page = virt_addr & ~0xfffUL;

    u64 access_type = 0;
    switch (scause) {
    case INSTRUCTION_ACCESS_FAULT:
        access_type = Generic_Mem_Region::Executable;
        break;
    case LOAD_PAGE_FAULT:
        access_type = Generic_Mem_Region::Readable;
        break;
    case STORE_AMO_PAGE_FAULT:
        access_type = Generic_Mem_Region::Writeable;
        break;
    }

    auto task       = get_cpu_struct()->current_task;
    auto page_table = task->page_table;

    try {
        if (scause == 7)
            throw Kern_Exception(-EIO, "Dodgy userspace");

        Auto_Lock_Scope lock(page_table->lock);

        auto &regions = page_table->paging_regions;
        auto it = regions.get_smaller_or_equal(page);
        if (it != regions.end() and it->is_in_range(virt_addr)) {
            // serial_logger.printf("Pagefault in region %s\n",
            // it->name.c_str());
            auto r = it->on_page_fault(access_type, virt_addr);
            if (not r)
                task->atomic_block_by_page(page, &task->page_table->blocked_tasks);
        } else
            throw Kern_Exception(-EFAULT, "pagefault in unknown region");
    } catch (const Kern_Exception &e) {
        serial_logger.printf("Warning: Pagefault %h pid %i (%s) rip %h error "
                             "%h -> %i killing process...\n",
                             virt_addr, task->task_id, task->name.c_str(), task->regs.pc, scause,
                             e.err_code);
        global_logger.printf("Warning: Pagefault %h pid %i (%s) pc %h error %h "
                             "-> %i killing process...\n",
                             virt_addr, task->task_id, task->name.c_str(), task->regs.pc, scause,
                             e.err_code);
        print_registers(task, serial_logger);
        task->atomic_kill();
    }
}

bool instruction_is_csr(u32 instruction)
{
    // SYSTEM
    return (instruction & 0x7f) == 0x73 and ((instruction >> 12) & 0x3) != 0x0;
}
bool instruction_is_fp_op(u32 instruction)
{
    // MADD, MSUB, NMSUB, NMADD and OP-FP
    return (instruction & 0x63) == 0x43 and (instruction & 0x1c) != 0x18 and
           (instruction & 0x1c) != 0x14;
}
bool instruction_is_fp_ld_st(u32 instruction)
{
    // C extension encoding (16 bit)
    bool compressed = ((instruction & 0x3) == 0b00 or (instruction & 0x3) == 0b10) and
                      ((instruction & 0x6000) == 0x2000);
    // 32 bit encoding
    bool full = (instruction & 0x7f) == 0x07 or (instruction & 0x7f) == 0x27;
    return compressed or full;
}
bool instruction_is_fp_csr(u32 instruction)
{
    u32 csr_imm = instruction >> 20;
    return instruction_is_csr(instruction) and
           (csr_imm == FFLAGS or csr_imm == FRM or csr_imm == FCSR);
}

void illegal_instruction(u32 instruction)
{
    auto task = get_cpu_struct()->current_task;

    try {
        if (instruction == 0) {
            bool b =
                copy_from_user((char *)&instruction, (char *)task->regs.pc, sizeof(instruction));
            if (not b) {
                task->atomic_block_by_page(task->regs.pc, &task->page_table->blocked_tasks);
                return;
            }
        }

        if (instruction_is_fp_csr(instruction)) {
            if (not fp_is_supported())
                throw Kern_Exception(-ENOTSUP, "Floating point not supported");

            restore_fp_state(task.get());
        } else if (instruction_is_fp_op(instruction) or instruction_is_fp_ld_st(instruction)) {
            if (not fp_is_supported())
                throw Kern_Exception(-ENOTSUP, "Floating point not supported");

            if (get_fp_state() != FloatingPointState::Disabled) // FP is enabled but instruction
                                                                // is illegal -> not supported
                throw Kern_Exception(-ENOTSUP,
                                     "Floating point width not supported");

            restore_fp_state(task.get());
        } else {
            // What is this
            throw Kern_Exception(-ENOTSUP, "Illegal instruction");
        }
    } catch (const Kern_Exception &e) {
        serial_logger.printf("Warning: %s pid %i (%s) pc %h instr 0x%h -> %i "
                             "killing process...\n",
                             e.err_message, task->task_id, task->name.c_str(), task->regs.pc,
                             instruction, e.err_code);
        global_logger.printf("Warning: %s pid %i (%s) pc %h instr 0x%h -> %i "
                             "killing process...\n",
                             e.err_message, task->task_id, task->name.c_str(), task->regs.pc,
                             instruction, e.err_code);
        task->atomic_kill();
    }
}

extern "C" void syscall_handler();

void service_timer_interrupt()
{
    sched_periodic();
    // Pending timer interrupt is cleared by SBI
}

void plic_service_interrupt()
{
    auto c = get_cpu_struct();

    const u32 irq = plic_claim();
    if (irq == 0) {
        // Spurious interrupt
        return;
    }

    // Get handler
    auto handler = c->int_handlers.get_handler(irq);
    if (handler == nullptr) {
        // Disable the interrupt
        plic_interrupt_disable(irq);
        plic_complete(irq);
        return;
    }

    // Send the interrupt to the port
    auto port = handler->port.lock();
    bool sent = false;
    try {
        if (port) {
            IPC_Kernel_Interrupt kmsg = {IPC_Kernel_Interrupt_NUM, irq, c->cpu_id};
            port->atomic_send_from_system(reinterpret_cast<char *>(&kmsg), sizeof(kmsg));
        }

        sent = true;
    } catch (const Kern_Exception &e) {
        serial_logger.printf("Error: %s\n", e.err_message);
    }

    if (not sent) {
        // Disable the interrupt
        plic_interrupt_disable(irq);
        plic_complete(irq);

        // Delete the handler
        c->int_handlers.remove_handler(irq);
    } else {
        handler->active = true;
    }
}

extern "C" void clear_soft_interrupt();

void service_software_interrupt()
{
    // The software interrupt is not cleared automatically
    clear_soft_interrupt();

    auto c = get_cpu_struct();

    u32 m = __atomic_fetch_and(&c->ipi_mask, ~CPU_Info::IPI_RESCHEDULE, __ATOMIC_SEQ_CST);
    if (m & CPU_Info::IPI_RESCHEDULE) {
        reschedule();
    }
}

void handle_interrupt()
{
    u64 scause, stval;
    get_scause_stval(&scause, &stval);

    auto c = get_cpu_struct();
    if (c->nested_level > 1) {
        serial_logger.printf("!!! kernel interrupt !!!\n");
        serial_logger.printf("nested level: %i\n", c->nested_level);
        serial_logger.printf("scause: 0x%x\n", scause);
        serial_logger.printf("stval: 0x%x\n", stval);
        serial_logger.printf("pc: 0x%x\n", c->current_task->regs.program_counter());
        print_stack_trace_fp(c->current_task->regs.s0);
        while (1)
            ;
    }

    if ((i64)scause < 0) {
        // Clear last bit
        u64 intno = scause & ~(1UL << 63);
        switch (intno) {
        case SOFTWARE_INTERRUPT:
            service_software_interrupt();
            break;
        case TIMER_INTERRUPT:
            service_timer_interrupt();
            break;
        case EXTERNAL_INTERRUPT:
            plic_service_interrupt();
            break;

        default:
            serial_logger.printf("Unknown interrupt: 0x%x\n", scause);
            while (1)
                ;
            break;
        }
    } else {
        switch (scause) {
        case ILLEGAL_INSTRUCTION:
            illegal_instruction(stval);
            break;
        case INSTRUCTION_PAGE_FAULT:
        case LOAD_PAGE_FAULT:
        case STORE_AMO_PAGE_FAULT:
            page_fault(stval, scause);
            break;
        case ENV_CALL_FROM_U_MODE:
            // Advance the program counter to the next instruction
            c->current_task->regs.program_counter() += 4;
            syscall_handler();
            break;
        case STORE_AMO_ACCESS_FAULT:
        case LOAD_ACCESS_FAULT:
        case INSTRUCTION_ACCESS_FAULT:
            page_fault(stval, scause);
            break;
        default:
            serial_logger.printf("Unknown exception: 0x%x\n", scause);
            while (1)
                ;
        }
    }

    while (c->current_task->regs.syscall_restart != 0) {
        syscall_handler();
    }

    assert(c->nested_level == 1);
}