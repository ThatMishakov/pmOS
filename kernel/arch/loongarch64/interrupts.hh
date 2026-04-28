#pragma once
#include <types.hh>

// TODO: Make this a namespace
constexpr u32 ESTAT_HARDWARE_INT_MASK = 0x3fc;
constexpr u32 ESTAT_TIMER_INT_MASK = 1 << 11;
constexpr u32 ESTAT_IPI_MASK = 1 << 12;
