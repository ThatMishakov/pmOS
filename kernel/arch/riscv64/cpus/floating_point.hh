#pragma once
#include <types.hh>

enum class FloatingPointSize {
    None = 0,
    SinglePrecision = 1,
    DoublePrecision = 2,
    QuadPrecision = 3,
};

/// Maximum supported floating point size by the system
/// Individual harts might theoretically support different sizes
// TODO: Assume heterogenous system for now
inline FloatingPointSize max_supported_fp_level = FloatingPointSize::None;

inline bool fp_is_supported()
{
    return max_supported_fp_level != FloatingPointSize::None;
}

constexpr int fp_register_size(FloatingPointSize size)
{
    switch (size) {
        case FloatingPointSize::None:
            return 0;
        case FloatingPointSize::SinglePrecision:
            return 32;
        case FloatingPointSize::DoublePrecision:
            return 64;
        case FloatingPointSize::QuadPrecision:
            return 128;
    }
    return 0;
}

enum FloatingPointState {
    Disabled = 0,
    Initial = 1,
    Clean = 2,
    Dirty = 3,
};

// Sets sstatus FS field to the given value
inline void set_fp_state(FloatingPointState state)
{
    u64 sstatus = 0;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus &= ~(0b11 << 13);
    sstatus |= (static_cast<u64>(state) << 13);
    asm volatile("csrw sstatus, %0" : : "r"(sstatus));
}

// Gets the current state of the floating point unit
inline FloatingPointState get_fp_state()
{
    u64 sstatus = 0;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    return static_cast<FloatingPointState>((sstatus >> 13) & 0b11);
}

struct TaskDescriptor;

// Restores the floating point state from the given task
void restore_fp_state(TaskDescriptor* task);

// Saves the floating point registers to the given pointer
void save_fp_registers(u64* fp_regs);