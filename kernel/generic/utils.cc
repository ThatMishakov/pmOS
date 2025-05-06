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

#include "utils.hh"

#include "types.hh"

#include <errno.h>
#include <kern_logger/kern_logger.hh>
#include <lib/string.hh>
#include <memory/paging.hh>
#include <messaging/messaging.hh>
#include <processes/tasks.hh>
#include <sched/sched.hh>
#include <stdarg.h>
#include <stdio.h>

using namespace kernel;
using namespace kernel::proc;
using namespace kernel::sched;
using namespace kernel::paging;
using namespace kernel::log;

void int_to_string(i64 n, u8 base, char *str, int &length)
{
    char temp_str[65];
    length        = 0;
    char negative = 0;
    if (n < 0) {
        negative = 1;
        str[0]   = '-';
        n *= -1;
    }
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        int tmp = n % base;
        n /= base;
        if (tmp < 10)
            temp_str[length] = tmp + '0';
        else
            temp_str[length] = tmp + 'A' - 10;

        ++length;
    }
    for (int i = 0; i < length; ++i) {
        str[i + negative] = temp_str[length - i - 1];
    }
    if (negative)
        ++length;
    str[length] = 0;
}

void uint_to_string(u64 n, u8 base, char *str, int &length)
{
    char temp_str[65];
    length = 0;
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        unsigned int tmp = n % base;
        n /= base;
        if (tmp > 9)
            temp_str[length] = tmp + 'A' - 10;
        else
            temp_str[length] = tmp + '0';

        ++length;
    }
    for (int i = 0; i < length; ++i) {
        str[i] = temp_str[length - i - 1];
    }
    str[length] = 0;
}

void printc(int);

void t_write_bochs(const char *str, u64 length)
{
    for (u64 i = 0; i < length; ++i) {
        printc(str[i]);
    }
}

void t_print_bochs(const char *str, ...)
{
    static Spinlock l;
    Auto_Lock_Scope ll(l);

    va_list arg;
    va_start(arg, str);

    char at        = str[0];
    unsigned int i = 0;
    while (at != '\0') {
        u64 s = i;
        while (true) {
            if (at == '\0') {
                va_end(arg);
                if (i - s > 0) {
                    t_write_bochs(str + s, i - s);
                }
                return;
            }
            if (at == '%')
                break;
            // term_write(str+i, 1);
            at = str[++i];
        }
        if (i - s > 0) {
            t_write_bochs(str + s, i - s);
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
        case 'x':
        case 'h': { // hexa number
            u64 casted_arg = va_arg(arg, u64);
            uint_to_string(casted_arg, 16, int_str_buffer, len);
            break;
        }
        case 's': {
            const char *ss = va_arg(arg, const char *);
            t_write_bochs(ss, strlen(ss));
            break;
        }
        }

        t_write_bochs(int_str_buffer, len);
        at = str[++i];
    }

    va_end(arg);
}

void term_write(const klib::string &s) { global_logger.log(s); }

ReturnStr<bool> prepare_user_buff_rd(const char *buff, size_t size)
{
    u64 addr_start = (u64)buff;
    u64 end        = addr_start + size;

    TaskDescriptor *current_task = get_current_task();

    if (buff > current_task->page_table->user_addr_max() or
        (void *) end > current_task->page_table->user_addr_max() or buff > (void *)end)
        return Error(-EFAULT);

    current_task->request_repeat_syscall();

    auto result = [&]() -> ReturnStr<bool> {
        for (u64 i = addr_start; i < end; ++i) {
            u64 page    = i & ~0xfffULL;
            auto result = current_task->page_table->prepare_user_page(
                (void *)page, Page_Table::Protection::Readable);
            if (!result.success())
                return result.propagate();

            if (not result.val) {
                current_task->atomic_block_by_page((void *)page,
                                                   &current_task->page_table->blocked_tasks);
                return false;
            }
        }
        return true;
    }();

    if (!result.success() || result.val)
        current_task->pop_repeat_syscall();

    return result;
}

ReturnStr<bool> prepare_user_buff_wr(char *buff, size_t size)
{
    u64 addr_start = (u64)buff;
    u64 end        = addr_start + size;

    TaskDescriptor *current_task = get_current_task();
    void *kern_addr_start        = current_task->page_table->user_addr_max();

    if (buff > kern_addr_start or (void *) end > kern_addr_start or buff > (void *)end)
        return Error(-EFAULT);

    auto result = [&]() -> ReturnStr<bool> {
        current_task->request_repeat_syscall();
        for (u64 i = addr_start; i < end; ++i) {
            u64 page = i & ~0xfffULL;
            auto t   = current_task->page_table->prepare_user_page((void *)page,
                                                                   Page_Table::Protection::Writeable);
            if (!t.success())
                return t.propagate();

            if (not t.val) {
                current_task->atomic_block_by_page((void *)page,
                                                   &current_task->page_table->blocked_tasks);
                return false;
            }
        }
        return true;
    }();

    if (!result.success() || result.val)
        current_task->pop_repeat_syscall();

    return result;
}

extern "C" void allow_access_user();
extern "C" void disallow_access_user();

extern "C" ReturnStr<bool> fast_copy_from_user(char *dest, const char *src, size_t size)
    __attribute__((weak));
ReturnStr<bool> copy_from_user(char *to, const char *from, size_t size)
{
    auto current_task = get_current_task();
    auto max_addr     = (uintptr_t)current_task->page_table->user_addr_max();
    uintptr_t addr    = (uintptr_t)from;
    if (addr + size > max_addr or addr > (UINTPTR_MAX - size))
        return Error(-EFAULT);

    if (fast_copy_from_user)
        return fast_copy_from_user(to, from, size);

    Auto_Lock_Scope l(current_task->page_table->lock);

    auto result = prepare_user_buff_rd(from, size);
    if (result.success() && result.val) {
        allow_access_user();
        memcpy(to, from, size);
        disallow_access_user();
    }

    return result;
}

extern "C" ReturnStr<bool> fast_copy_to_user(char *dest, const char *src, size_t size)
    __attribute__((weak));
ReturnStr<bool> copy_to_user(const char *from, char *to, size_t size)
{
    auto current_task = get_current_task();
    auto max_addr     = (uintptr_t)current_task->page_table->user_addr_max();
    uintptr_t addr    = (uintptr_t)to;
    if (addr + size > max_addr or addr > (UINTPTR_MAX - size))
        return Error(-EFAULT);

    if (fast_copy_to_user)
        return fast_copy_to_user(to, from, size);

    Auto_Lock_Scope l(current_task->page_table->lock);

    auto result = prepare_user_buff_wr(to, size);
    if (result.success() && result.val) {
        allow_access_user();
        memcpy(to, from, size);
        disallow_access_user();
    }

    return result;
}

extern "C" void print_stack_trace();

void *memcpy(void *__restrict d, const void *__restrict s, size_t n)
{
    char *dest      = (char *)d;
    const char *src = (const char *)s;

    void *k = dest;
    while (n--) {
        *((char *)dest++) = *((char *)src++);
    }

    return k;
}

extern "C" int memcmp(const void *s1, const void *s2, size_t n)
{
    const char *c1 = (const char *)s1;
    const char *c2 = (const char *)s2;

    for (size_t i = 0; i < n; ++i) {
        if (c1[i] != c2[i])
            return c1[i] - c2[i];
    }

    return 0;
}

extern "C" size_t strlen(const char *start)
{
    if (start == nullptr)
        return 0;

    const char *end = start;

    for (; *end != '\0'; ++end)
        ;

    return end - start;
}

// void copy_frame(u64 from, u64 to)
// {
//     copy_frame_s.lock();
//     u64 t1 = copy_frame_free_p.get_free_page();
//     u64 t2 = copy_frame_free_p.get_free_page();

//     Page_Table_Arguments pta = {1, 0, 0, 1, 0b010};
//     map(from, t1, pta);
//     map(to, t2, pta);

//     memcpy((char*)t1, (char*)t2, 4096);

//     release_page_s(*get_pte(t1, rec_map_index));
//     release_page_s(*get_pte(t2, rec_map_index));

//     invalidade(t1);
//     invalidade(t2);

//     copy_frame_free_p.release_free_page(t1);
//     copy_frame_free_p.release_free_page(t2);
//     copy_frame_s.unlock();
// }

// struct stack_frame {
//     stack_frame* next;
//     void* return_addr;
// };

// void print_stack_trace(Logger& logger)
// {
//     unw_context_t uc;
// 	unw_getcontext(&uc);

// 	unw_cursor_t cursor;
// 	unw_init_local(&cursor, &uc);

//     while(unw_step(&cursor)>0) {
// 		unw_word_t ip;
// 		unw_get_reg(&cursor, UNW_REG_IP, &ip);

// 		unw_word_t offset;
// 		char name[32];

//         unw_get_proc_name(&cursor, name,sizeof(name), &offset);

//         logger.printf("  0x%x at <%s>+0x%x\n", ip, name, offset);
//     }
// }

// void print_stack_trace(Logger& logger, stack_frame * s)
// {
//     logger.printf("Stack trace:\n");
//     for (; s != NULL; s = s->next)
//         logger.printf("  -> %h\n", (u64)s->return_addr);
// }

// Arch-specific!
// void print_stack_trace(Logger& logger)
// {
//     struct stack_frame* s;
//     __asm__ volatile("movq %%rbp, %0": "=a" (s));
//     print_stack_trace(logger, s);
// }

// extern "C" void serial_print_stack_trace(void * s)
// {
//     print_stack_trace(serial_logger, (stack_frame *)s);
// }

extern "C" void dbg_uart_init();

extern void hcf();
extern "C" void abort(void)
{
    t_print_bochs("Error: abort() was called.\n");

    print_stack_trace();
    // serial_logger.printf("Error: abort() was called.\n");

    // print_stack_trace(bochs_logger);
    // print_stack_trace(serial_logger);

    // serial_logger.printf("Dropping into debugger...\n---------------\n");

    // breakpoint;

    hcf();
}

void panic(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    serial_logger.vprintf(msg, args);
    va_end(args);

    serial_logger.printf("\n");
    abort();
}

extern "C" void _assert_fail(const char *condition, const char *file, unsigned int line)
{
    serial_logger.printf("Assert fail %s in file %s at line %i\n", condition, file, line);

    abort();
}

extern "C" void *memset(void *str, int c, size_t n)
{
    unsigned char *cc = (unsigned char *)str;
    for (size_t i = 0; i < n; ++i)
        cc[i] = c;

    return str;
}

void clear_page(u64 phys_addr, u64 pattern)
{
    Temp_Mapper_Obj<u64> mapper(request_temp_mapper());
    mapper.map(phys_addr);

    for (size_t i = 0; i < 4096 / sizeof(u64); ++i)
        mapper.ptr[i] = pattern;
}

int fflush(FILE *) { return 0; }

int fprintf(FILE *, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    bochs_logger.vprintf(format, args);

    va_end(args);
    return 0;
}

// Arch-specific spinlock functions
void acquire_lock_spin(u32 *lock) noexcept;
void release_lock(u32 *lock) noexcept;
bool try_lock(u32 *lock) noexcept;

void lock_var(unsigned *var) { acquire_lock_spin(var); }

void unlock_var(unsigned *var) { release_lock(var); }

extern "C" int strcmp(const char *str1, const char *str2)
{
    while (*str1) {
        if (*str1 != *str2)
            break;

        ++str1;
        ++str2;
    }

    return (int)*str1 - (int)*str2;
}

void copy_from_phys(u64 phys_addr, void *to, size_t size)
{
    Temp_Mapper_Obj<void> mapper(request_temp_mapper());
    for (u64 i = phys_addr & ~0xfffULL; i < phys_addr + size; i += 0x1000) {
        mapper.map(i);
        const u64 start = i < phys_addr ? phys_addr : i;
        const u64 end   = i + 0x1000 < phys_addr + size ? i + 0x1000 : phys_addr + size;
        memcpy((char *)to + (start - phys_addr), (char *)mapper.ptr + (start - i), end - start);
    }
}

size_t strlen_over_phys(u64 phys_addr)
{
    Temp_Mapper_Obj<char> mapper(request_temp_mapper());

    size_t len = 0;
    for (u64 i = phys_addr & ~0xfffULL;; i += 0x1000) {
        mapper.map(i);
        const u64 start = i < phys_addr ? phys_addr : i;
        const u64 end   = i + 0x1000;
        for (u64 j = start; j < end; ++j) {
            if (mapper.ptr[j - i] == '\0')
                return len;
            ++len;
        }
    }
}

klib::string capture_from_phys(u64 phys_addr)
{
    auto len = strlen_over_phys(phys_addr);
    klib::string s(len, ' ');
    copy_from_phys(phys_addr, const_cast<char *>(s.data()), len);
    return s;
}

#include <assert.h>

extern "C" void __floatundidf()
{
    // stub
    assert(false);
}

extern "C" void __cxa_pure_virtual() { panic("__cxa_pure_virtual()"); }

using guard_type = u64;

extern "C" int __cxa_guard_acquire(int64_t *guard_object)
{
    int32_t expected = 0;
    auto asint       = reinterpret_cast<int32_t *>(guard_object);
    while (true) {
        if (asint[1])
            continue;

        if (__atomic_compare_exchange_n(asint, &expected, 1, false, __ATOMIC_ACQUIRE,
                                        __ATOMIC_ACQUIRE)) {
            if (*guard_object == 0)
                return 1;

            __atomic_store_n(asint, 0, __ATOMIC_RELEASE);
            return 0;
        }
    }
}

extern "C" void __cxa_guard_release(int64_t *guard_object)
{
    auto asint = reinterpret_cast<int32_t *>(guard_object);
    asint[0]   = 1;
    __atomic_store_n(asint + 1, 0, __ATOMIC_RELEASE);
}

// Defined in sched/sched.cc
namespace kernel::sched
{
void check_synchronous_ipis();
}

void Spinlock::lock() noexcept
{
    while (!Spinlock_base::try_lock()) {
        // Since the current kernel spinlock situation is not very good, and the
        // kernel can't preempt itself, if the lock fails to acquire,
        // check that other CPUs are not waiting for us to do something.
        check_synchronous_ipis();
    }
}

bool Spinlock::try_lock() noexcept
{
    auto result = Spinlock_base::try_lock();
    if (!result) {
        check_synchronous_ipis();
    }
    return result;
}

// Halt and catch fire function.
void hcf(void)
{
// should be arch-specific, but whatever...
#if defined(__x86_64__) || defined(__i386__)
    asm("cli");
    for (;;) {
        asm("hlt");
    }
#elif defined(__riscv)
    asm volatile("wfi");
#endif

    while (1)
        ;

    __builtin_unreachable();
}

#if UINT32_MAX == UINTPTR_MAX
    #define STACK_CHK_GUARD 0xe2dee396
#else
    #define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

uintptr_t __stack_chk_guard = STACK_CHK_GUARD;

__attribute__((noreturn)) extern "C" void __stack_chk_fail(void)
{
    panic("Stack smashing detected");
}

extern "C" ReturnStr<bool> user_access_page_fault(unsigned access, const char *faulting_addr,
                                                  char *&dst, char *&src, size_t &size)
{
    TaskDescriptor *current_task = get_current_task();

    ReturnStr<bool> result =
        current_task->page_table->prepare_user_page((void *)faulting_addr, access);
    if (!result.success()) {
        return result;
    }

    if (!result.val) {
        current_task->atomic_block_by_page((void *)faulting_addr,
                                           &current_task->page_table->blocked_tasks);
        return false;
    }

    if (access == Protection::Readable) {
        assert(faulting_addr >= src);
        size_t diff = faulting_addr - src;
        assert(diff < size);
        dst += diff;
        src += diff;
        size += diff;
    } else {
        assert(access == Protection::Writeable);
        assert(faulting_addr >= dst);
        size_t diff = faulting_addr - dst;
        assert(diff < size);
        dst += diff;
        src += diff;
        size += diff;
    }

    return result;
}

ReturnStr<std::optional<klib::vector<char>>> to_buffer_from_user(void *ptr, size_t size)
{
    klib::vector<char> data;
    if (!data.resize(size))
        return Error(-ENOMEM);

    auto result = copy_from_user(&data.front(), (char *)ptr, size );
    if (!result.success())
        return result.propagate();

    if (!result.val)
        return Success(std::nullopt);

    return Success(std::move(data));
}