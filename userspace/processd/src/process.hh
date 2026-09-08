#pragma once
#include <array>
#include <cinttypes>

struct Sigaction {
    uint64_t sa_handler = 0; // SIG_DFL
    uint64_t sa_restorer = 0;
    uint64_t sa_mask = 0;
    uint32_t sa_flags = 0;
};

struct Process {
    std::array<Sigaction, 64> sigactions = {};

    int32_t pid = 0;
};