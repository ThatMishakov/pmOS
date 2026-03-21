#pragma once
#include <types.hh>

namespace kernel::x86::kvmclock
{

constexpr u32 MSR_KVM_WALL_CLOCK      = 0x4b564d00;
constexpr u32 MSR_KVM_SYSTEM_TIME     = 0x4b564d01;
constexpr u32 MSR_KVM_WALL_CLOCK_OLD  = 0x11;
constexpr u32 MSR_KVM_SYSTEM_TIME_OLD = 0x12;

struct pvclock_vcpu_time_info {
    u32 version;
    u32 pad0;
    u64 tsc_timestamp;
    u64 system_time;
    u32 tsc_to_system_mul;
    i8 tsc_shift;
    u8 pad[3];
};

void init_kvmclock();

} // namespace kernel::x86::kvmclock