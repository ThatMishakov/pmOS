#include "kern_logger.hh"
#include <types.hh>
#include <stdarg.h>
#include <messaging/messaging.hh>

Buffered_Logger global_logger;
Bochs_Logger bochs_logger;

void Logger::vprintf(const char* str, va_list arg)
{
    Auto_Lock_Scope scope_lock(logger_lock);

    char at = str[0];
    unsigned int i = 0;
    while (at != '\0') {
        u64 s = i;
        while (true) {
            if (at == '\0') {
                if (i - s > 0) {
                    log_nolock(str + s, i - s);
                }
                return;
            }
            if (at == '%') break;
            //term_write(str+i, 1);
            at = str[++i];
        }
        if (i - s > 0) {
            log_nolock(str + s, i - s);
        }
       
        at = str[++i]; // char next to %
        char int_str_buffer[32];
        int len = 0;
        switch (at) {
            case 'i': { // signed integer
                i64 casted_arg = va_arg(arg, i64);
                int_to_string(casted_arg, 10, int_str_buffer, len);
                break;
            }
            case 'u': { // unsigned integer
                u64 casted_arg = va_arg(arg, u64);
                uint_to_string(casted_arg, 10, int_str_buffer, len);
                break;
            }
            case 'h': { // hexa number
                u64 casted_arg = va_arg(arg, u64);
                uint_to_string(casted_arg, 16, int_str_buffer, len);
                break;
            }
            case 's': {
                const char *ss = va_arg(arg, const char *);
                log_nolock(ss, strlen(ss));
                // t_write_bochs(ss, strlen(ss));
                break;
            }
        }

        log_nolock(int_str_buffer, len);
        at = str[++i];
    }
}

void Logger::printf(const char* str, ...)
{
    va_list arg;
    va_start(arg, str);

    vprintf(str, arg);

    va_end(arg);
}

void Logger::log(const char* str, size_t size)
{
    Auto_Lock_Scope local_lock(logger_lock);

    log_nolock(str, size);
}

void Logger::log(const klib::string& str)
{
    Auto_Lock_Scope local_lock(logger_lock);

    log_nolock(str.c_str(), str.size());
}

void t_write_bochs(const char * str, u64 length);

void Bochs_Logger::log_nolock(const char* c, size_t size)
{
    t_write_bochs(c, size);
}

void Buffered_Logger::log_nolock(const char* c, size_t size)
{
    constexpr size_t buff_size = 508;

    klib::shared_ptr<Port> ptr;

    if (ptr = messaging_port.lock()) {
        struct Msg {
            u32 type = 0x40;
            char buff[buff_size];
        } var;


        size_t length = size;
        const char* str = c;

        for (size_t i = 0; i < length; i += buff_size) {
            size_t l = min(length - i, buff_size);
            memcpy(var.buff, str, l);
            ptr->atomic_send_from_system((char*)&var, l+4);
        }
    } else {
        log_buffer.append(c, size);
    }
}

kresult_t Buffered_Logger::set_port(const klib::shared_ptr<Port>& port, uint32_t flags)
{
    Auto_Lock_Scope scope_lock(logger_lock);

    messaging_port = port;

    Auto_Lock_Scope port_lock(port->lock);

    // TODO

    constexpr size_t buff_size = 508;

    struct Msg {
        u32 type = 0x40;
        char buff[buff_size];
    } var;


    size_t length = log_buffer.length();
    const char* str = log_buffer.c_str();

    for (size_t i = 0; i < length; i += buff_size) {
        size_t length = min(length - i, buff_size);
        memcpy(var.buff, str, length);
        kresult_t r;
        if ((r = port->send_from_system((char*)&var, length+4)) != SUCCESS)
            return r;
    }

    log_buffer.clear();

    return SUCCESS;
}