#pragma once
#include <types.hh>

// TODO: Make this a namespace

constexpr u32 ESTAT_HARDWARE_INT_MASK = 0xff << 2; 
constexpr u32 ESTAT_TIMER_INT_MASK = 1 << 11;
constexpr u32 ESTAT_IPI_MASK = 1 << 12;

constexpr u32 ESTAT_INTERRUPTS_MASK = ESTAT_HARDWARE_INT_MASK | ESTAT_TIMER_INT_MASK | ESTAT_IPI_MASK;
