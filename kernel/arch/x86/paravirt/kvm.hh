#pragma once
#include <types.hh>

namespace kernel::x86::kvm
{

constexpr u32 KVM_CPUID_SIGNATURE = 0x40000000;
constexpr u32 KVM_CPUID_FEATURES = 0x40000001;

constexpr u32 KVM_FEATURE_CLOCKSOURCE = (1 << 0);
constexpr u32 KVM_FEATURE_CLOCKSOURCE2 = (1 << 3);

u32 kvm_cpuid_max_leaf();

}