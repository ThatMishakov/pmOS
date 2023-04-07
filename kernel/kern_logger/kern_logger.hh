#pragma once
#include <messaging/messaging.hh>
#include <lib/memory.hh>
#include <types.hh>
#include <lib/string.hh>
#include <stdarg.h>

/// @brief A generic logger object, which is then extended into different log handlers, available in different environments.
struct Logger {
    Spinlock logger_lock;

    /// C standart library-like printf, which prints to the logger. Locks logger_lock for thread safety.
    void printf(const char* format, ...);

    /// C standart library-like vprintf, which prints to the logger. Locks logger_lock for thread safety.
    void vprintf(const char* format, va_list list);

    void log(const klib::string& s);
    void log(const char* s, size_t size);
    virtual ~Logger() = default;

    virtual void log_nolock(const char* c, size_t size) = 0;
};

/**
 * @brief A global buffered kernel logger.
 * 
 * This is a default logger where the kernel should write its information logs and whatnot. It is backed by an infinite,
 * dynamically-growing buffer where the messages are stored if the messaging_port is unavailable. If it is available, the
 * messages are automatically sent there with the IPC_Write_Plain messages. The port can be set by issuing syscall_set_log_port()
 * 
 * @see syscall_set_log_port()
 */
struct Buffered_Logger: Logger {
    klib::string log_buffer;
    klib::weak_ptr<Port> messaging_port;

    virtual void log_nolock(const char* c, size_t size) override;

    void set_port(const klib::shared_ptr<Port>& port, uint32_t flags);
};
extern Buffered_Logger global_logger;

/**
 * @brief Bochs magic debug logger
 * 
 * This logger only works in bochs and works by writing the messages to the 0xe9 CPU port. It is useless on real systems
 * but is exctimely convinient for casual debugging with bochs.
 */
struct Bochs_Logger: Logger {
    virtual void log_nolock(const char* c, size_t size) override;
};
extern Bochs_Logger bochs_logger;

class Serial_Logger : public Logger {
public:
    void log_nolock(const char* c, size_t size) override;
    virtual ~Serial_Logger() = default;
};

inline Serial_Logger serial_logger;