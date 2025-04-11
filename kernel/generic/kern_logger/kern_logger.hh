/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <lib/memory.hh>
#include <lib/string.hh>
#include <messaging/messaging.hh>
#include <stdarg.h>
#include <types.hh>

namespace kernel::log
{

/// @brief A generic logger object, which is then extended into different log handlers, available in
/// different environments.
struct Logger {
    // deadlock on TLB shootdown ? (noting this to think about it later)
    CriticalSpinlock logger_lock;

    /// C standart library-like printf, which prints to the logger. Locks logger_lock for thread
    /// safety.
    void printf(const char *format, ...);

    /// C standart library-like vprintf, which prints to the logger. Locks logger_lock for thread
    /// safety.
    void vprintf(const char *format, va_list list);

    void log(const klib::string &s);
    void log(const char *s, size_t size);
    virtual ~Logger() = default;

    virtual void log_nolock(const char *c, size_t size) = 0;
};

/**
 * @brief A global buffered kernel logger.
 *
 * This is a default logger where the kernel should write its information logs and whatnot. It is
 * backed by an infinite, dynamically-growing buffer where the messages are stored if the
 * messaging_port is unavailable. If it is available, the messages are automatically sent there with
 * the IPC_Write_Plain messages. The port can be set by issuing syscall_set_log_port()
 *
 * @see syscall_set_log_port()
 */
struct Buffered_Logger: Logger {
    klib::string log_buffer;
    u64 messaging_port_id = 0;

    virtual void log_nolock(const char *c, size_t size) override;

    void set_port(ipc::Port *port, uint32_t flags);
};
extern Buffered_Logger global_logger;

/**
 * @brief Bochs magic debug logger
 *
 * This logger only works in bochs and works by writing the messages to the 0xe9 CPU port. It is
 * useless on real systems but is exctimely convinient for casual debugging with bochs.
 */
struct Bochs_Logger: Logger {
    virtual void log_nolock(const char *c, size_t size) override;
};
extern Bochs_Logger bochs_logger;

class Serial_Logger final: public Logger
{
public:
    void log_nolock(const char *c, size_t size) override;
    virtual ~Serial_Logger() = default;
};

inline Serial_Logger serial_logger;

}; // namespace kernel::log