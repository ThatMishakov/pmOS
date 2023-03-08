#pragma once
#include <messaging/messaging.hh>
#include <lib/memory.hh>
#include <types.hh>
#include <lib/string.hh>
#include <stdarg.h>

struct Logger {
    Spinlock logger_lock;

    void printf(const char* format, ...);
    void vprintf(const char* format, va_list list);

    void log(const klib::string& s);
    void log(const char* s, size_t size);
    virtual ~Logger() = default;

    virtual void log_nolock(const char* c, size_t size) = 0;
};

struct Buffered_Logger: Logger {
    klib::string log_buffer;
    klib::weak_ptr<Port> messaging_port;

    virtual void log_nolock(const char* c, size_t size) override;

    void set_port(const klib::shared_ptr<Port>& port, uint32_t flags);
};

struct Bochs_Logger: Logger {
    virtual void log_nolock(const char* c, size_t size) override;
};

extern Buffered_Logger global_logger;
extern Bochs_Logger bochs_logger;