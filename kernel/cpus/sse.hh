#pragma once
#include <types.hh>

struct SSE_Data {
    char fxsave_region[512] ALIGNED(16) = {0};

    inline void save_sse()
    {
        asm volatile(" fxsave %0 "::"m"(fxsave_region));
    }


    inline void restore_sse()
    {
        asm volatile(" fxrstor %0 "::"m"(fxsave_region));
    }
};

// Enables SSE support
void enable_sse();

// Check if SSE state is valid (CR0.TS == 0)
bool sse_is_valid();

// Invalidates/validates SSE
void invalidate_sse();
void validate_sse();