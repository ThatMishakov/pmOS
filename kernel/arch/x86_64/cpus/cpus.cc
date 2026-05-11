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

#include <sched/sched.hh>
#include <syscall.hh>
#include <x86_asm.hh>

using namespace kernel;
using namespace kernel::sched;
using namespace kernel::x86;

bool setup_stacks(sched::CPU_Info *c)
{
    assert(c);

    TSS *tss = (TSS *)c->tss_virt;
    *tss = {};

    c->cpu_gdt.tss_descriptor = System_Segment_Descriptor((u64)tss, PAGE_SIZE - 1, 0x89, 0x02);

    c->kernel_stack_top = c->kernel_stack.get_stack_top();

    c->cpu_gdt.tss_descriptor.tss()->ist7 = (u64)c->double_fault_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist6 = (u64)c->nmi_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist5 = (u64)c->machine_check_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist2 = (u64)c->debug_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->ist1 = (u64)c->kernel_stack.get_stack_top();
    c->cpu_gdt.tss_descriptor.tss()->rsp0 = (u64)c->kernel_stack.get_stack_top();

    return true;
}

namespace kernel::x86::gdt {

void io_bitmap_enable()
{
    auto c = sched::get_cpu_struct();
    c->cpu_gdt.tss_descriptor = System_Segment_Descriptor((u64)c->tss_virt, PAGE_SIZE*3 - 1, 0x89, 0x02);
    loadTSS(TSS_SEGMENT);
}

void io_bitmap_disable()
{
    auto c = sched::get_cpu_struct();
    c->cpu_gdt.tss_descriptor = System_Segment_Descriptor((u64)c->tss_virt, PAGE_SIZE - 1, 0x89, 0x02);
    loadTSS(TSS_SEGMENT);
}

}

#include "syscall.hh"
#include <x86_asm.hh>

constexpr u64 IA32_EFER = 0xC0000080;
constexpr u32 IA32_STAR = 0xC0000081;
constexpr u32 IA32_LSTAR = 0xC0000082;
constexpr u32 IA32_CSTAR = 0xC0000083;
constexpr u32 IA32_FMASK = 0xC0000084;

namespace kernel::x86 {

void program_syscall()
{
    write_msr(IA32_STAR, ((u64)(R0_CODE_SEGMENT) << 32) | ((u64)(R3_LEGACY_CODE_SEGMENT) << 48));
    // STAR (segments for user and kernel code)
    write_msr(IA32_LSTAR, (u64)&syscall_entry); // LSTAR (64 bit entry point)
    write_msr(IA32_FMASK, (u32)~0x0);                     // SFMASK (mask for %rflags)

    // Enable SYSCALL/SYSRET in EFER register
    u64 efer = read_msr(IA32_EFER);
    write_msr(IA32_EFER, efer | (0x01 << 0));

    // This doesn't work on AMD CPUs
    // u64 cpuid = ::cpuid(1);
    // if (cpuid & (u64(1) << (11 + 32))) {
    //     write_msr(0x174, R0_CODE_SEGMENT); // IA32_SYSENTER_CS
    //     write_msr(0x175, (u64)get_cpu_struct()->kernel_stack_top); //
    //     IA32_SYSENTER_ESP write_msr(0x176, (u64)&sysenter_entry); //
    //     IA32_SYSENTER_EIP
    // }
}

}