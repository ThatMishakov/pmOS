#pragma once

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

constexpr int fp_register_size(FloatingPointSize size) {
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