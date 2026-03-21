#include "timers.hh"
#include <paravirt/kvm.hh>
#include <x86_utils.hh>
#include <x86_asm.hh>
#include <kern_logger/kern_logger.hh>
#include <memory/paging.hh>
#include <memory/vmm.hh>
#include "kvmclock.hh"

using namespace kernel;
using namespace kernel::x86::kvm;

namespace kernel::x86::kvmclock
{

volatile pvclock_vcpu_time_info *kvmclock = nullptr;

void init_kvmclock()
{
    if (kvm_cpuid_max_leaf() < 0x40000001)
        // Not running inside KVM...
        return;

    u32 msr = 0;

    auto c = cpuid(KVM_CPUID_FEATURES);
    if (c.eax & KVM_FEATURE_CLOCKSOURCE2) {
        log::serial_logger.printf("Found KVM clock!\n");
        msr = MSR_KVM_SYSTEM_TIME;
    } else if (c.eax & KVM_FEATURE_CLOCKSOURCE) {
        log::serial_logger.printf("Found KVM clock (at old MSR)!\n");
        msr = MSR_KVM_SYSTEM_TIME_OLD;
    } else {
        return;
    }

    auto page_phys = pmm::get_memory_for_kernel(1);
    if (page_phys == -1UL)
        panic("Failed to allocate memory for the kernel for kvmclock");

    void *vaddr = vmm::kernel_space_allocator.virtmem_alloc(1);
    if (!vaddr)
        panic("Failed to alloc virt space for kvmclock!\n");

    paging::Page_Table_Arguments pta = {
        .readable           = true,
        .writeable          = false,
        .user_access        = false,
        .global             = true,
        .execution_disabled = true,
        .extra              = 0,
    };
    auto result = map_kernel_page(page_phys, vaddr, pta);
    if (result)
        panic("Failed to map memory for the kvmclock...\n");

    write_msr(msr, page_phys);
    kvmclock = (pvclock_vcpu_time_info *)vaddr;
}

// u64 pvclock_read_ms()
// {
//     u32 version;
//     u64 tsc_timestamp;
//     u64 system_time;
//     u32 tsc_to_system_mul;
//     i8 tsc_shift;
//     do {
//         version = kvmclock->version;
//         tsc_timestamp = kvmclock->tsc_timestamp;
//         system_time = kvmclock->system_time;
//         tsc_to_system_mul = kvmclock->tsc_to_system_mul;
//         tsc_shift = kvmclock->tsc_shift;
//     } while (version & 0x01 and version != kvmclock->version);

//     u64 tsc = rdtsc();
    
// }

}