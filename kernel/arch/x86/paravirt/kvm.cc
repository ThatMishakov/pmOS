#include "kvm.hh"
#include <x86_utils.hh>

#include <kern_logger/kern_logger.hh>

namespace kernel::x86::kvm {

u32 kvm_cpuid_max_leaf()
{
    auto c = cpuid(KVM_CPUID_SIGNATURE);
    if (c.ebx == 0x4b4d564b and c.ecx == 0x564b4d56 and c.edx == 0x4d)
        return c.eax;
    return 0;
}

}