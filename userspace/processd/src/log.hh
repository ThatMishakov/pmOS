#pragma once

#include <frg/logging.hpp>

struct KernelSink {
    constexpr KernelSink() = default;
    void operator() (const char *message);
};

inline frg::stack_buffer_logger<KernelSink, 512> kernelLogger;