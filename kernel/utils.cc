#include "utils.hh"
#include <stdarg.h>
#include "types.hh"
#include <kernel/errors.h>
#include <memory/paging.hh>
#include <messaging/messaging.hh>
#include <lib/string.hh>
#include <kern_logger/kern_logger.hh>
#include <sched/sched.hh>
#include <stdio.h>
#include <libunwind.h>

void int_to_string(long int n, u8 base, char* str, int& length)
{
    char temp_str[65];
    length = 0;
    char negative = 0;
    if (n < 0) {
        negative = 1;
        str[0] = '-';
        n *= -1;
    }
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        int tmp = n%base;
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
    if (negative) ++length;
    str[length] = 0;
}

void uint_to_string(unsigned long int n, u8 base, char* str, int& length)
{
    char temp_str[65];
    length = 0;
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        unsigned int tmp = n%base;
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

void t_write_bochs(const char * str, u64 length)
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

    char at = str[0];
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
            if (at == '%') break;
            //term_write(str+i, 1);
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

void term_write(const klib::string& s)
{
    global_logger.log(s);
}

bool prepare_user_buff_rd(const char* buff, size_t size)
{
    u64 addr_start = (u64)buff;
    u64 end = addr_start+size;

    klib::shared_ptr<TaskDescriptor> current_task = get_current_task();

    if (addr_start > current_task->page_table->user_addr_max() or end > current_task->page_table->user_addr_max() or addr_start > end)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "user parameter is out of range"));


    try {
        current_task->request_repeat_syscall();

        for (u64 i = addr_start; i < end; ++i) {
            u64 page = i & ~0xfffULL;
            bool result = current_task->page_table->prepare_user_page(page, Page_Table::Readable);
            if (not result)
                current_task->atomic_block_by_page(page, &current_task->page_table->blocked_tasks);
            if (not result)
                return result;
        }
        
        current_task->pop_repeat_syscall();
    } catch (...) {
        current_task->pop_repeat_syscall();
        throw;
    }

    return true;
}

bool prepare_user_buff_wr(char* buff, size_t size)
{
    u64 addr_start = (u64)buff;
    u64 end = addr_start+size;

    bool avail = true;

    klib::shared_ptr<TaskDescriptor> current_task = get_current_task();
    u64 kern_addr_start = current_task->page_table->user_addr_max();

    if (addr_start > kern_addr_start or end > kern_addr_start or addr_start > end)
        throw(Kern_Exception(ERROR_OUT_OF_RANGE, "prepare_user_buff_wr outside userspace"));

    try {
        current_task->request_repeat_syscall();
        for (u64 i = addr_start; i < end and avail; ++i) {
            u64 page = i & ~0xfffULL;
            avail = current_task->page_table->prepare_user_page(page, Page_Table::Writeable);
            if (not avail)
                current_task->atomic_block_by_page(page, &current_task->page_table->blocked_tasks);
        }
        if (avail)
            current_task->pop_repeat_syscall();
    } catch (...) {
        current_task->pop_repeat_syscall();
        throw;
    }

    return avail;
}

bool copy_from_user(char* to, const char* from, size_t size)
{
    bool result = prepare_user_buff_rd(from, size);
    if (result)
        memcpy(to, from, size);

    return result;
}

bool copy_to_user(const char* from, char* to, size_t size)
{
    bool result = prepare_user_buff_wr(to, size);
    if (result)
        memcpy(to, from, size);

    return result;
}

extern "C" void print_stack_trace();

void memcpy(char* to, const char* from, size_t size)
{
    //t_print_bochs("memcpy %h <- %h size %h\n", to, from, size);
    //print_stack_trace();

    for (size_t i = 0; i < size; ++i) {
        to[i] = from[i];
    }
}

extern "C" size_t strlen(const char *start)
{
    if (start == nullptr) return 0;

    const char* end = start;

    for (; *end != '\0'; ++end)
        ;

    return end - start;
}

// void copy_frame(u64 from, u64 to)
// {
//     copy_frame_s.lock();
//     u64 t1 = copy_frame_free_p.get_free_page();
//     u64 t2 = copy_frame_free_p.get_free_page();


//     Page_Table_Argumments pta = {1, 0, 0, 1, 0b010};
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

struct stack_frame {
    stack_frame* next;
    void* return_addr;
};

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

void print_stack_trace(Logger& logger, stack_frame * s)
{
    logger.printf("Stack trace:\n");
    for (; s != NULL; s = s->next)
        logger.printf("  -> %h\n", (u64)s->return_addr);
}

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

extern "C" void abort(void)
{
    t_print_bochs("Error: abort() was called. Hint: use debugger on COM1\n");

    print_stack_trace();
    serial_logger.printf("Error: abort() was called.\n");

    //print_stack_trace(bochs_logger);
    //print_stack_trace(serial_logger);

    serial_logger.printf("Dropping into debugger...\n---------------\n");

    //breakpoint;

    while (1);
}

extern "C" void _assert_fail(const char* condition, const char* file, unsigned int line)
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

    for (size_t i = 0; i < 4096/sizeof(u64); ++i)
        mapper.ptr[i] = pattern;
}

int fflush(FILE *stream)
{

}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    bochs_logger.vprintf(format, args);

    va_end(args);
    return 1;
}

// Arch-specific spinlock functions
extern "C" {
    void acquire_lock_spin(u32 *lock) noexcept;
    void release_lock(u32 *lock) noexcept;
    bool try_lock(u32 *lock) noexcept;
}

void lock_var(unsigned *var)
{
    acquire_lock_spin(var);
}

void unlock_var(unsigned *var)
{
    release_lock(var);
}

#include <pthread.h>

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    if (rwlock->b == 0) {
        unlock_var(&rwlock->g);
    } else {
        lock_var(&rwlock->r);
        rwlock->b = rwlock->b - 1;
        if (rwlock->b == 0)
            unlock_var(&rwlock->g);

        unlock_var(&rwlock->r);
    }
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    lock_var(&rwlock->r);
    rwlock->b = rwlock->b + 1;
    if (rwlock->b == 1)
        lock_var(&rwlock->g);

    unlock_var(&rwlock->r);
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    lock_var(&rwlock->g);
    return 0;
}

extern "C" char *getenv(const char *name)
{
    // if (klib::string(name) == "LIBUNWIND_PRINT_UNWINDING")
    //     return (char*)"1";

    return NULL;
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
    bool r = false;
    do {
        if (*mutex)
            continue;

        r = __sync_bool_compare_and_swap(mutex, 0, 1);
    } while (not r);
    __sync_synchronize();

    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    *mutex = 0;

    return 0;
}

// I have no idea if it works
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    pthread_mutex_unlock(mutex);

    bool r = false;
    do {
        if (*cond == 0)
            continue;

        r = __sync_bool_compare_and_swap(cond, 1, 0);
    } while (not r);
    __sync_synchronize();

    pthread_mutex_lock(mutex);

    return 0;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
    *cond = 1;

    return 0;
}

struct Pthread_Once_Global_S {
    void (*destructor)(void*) = nullptr;
    bool valid = false;
};

klib::array<Pthread_Once_Global_S, CPU_Info::pthread_once_size> pthread_global = {};
Spinlock pthread_key_lock;

extern "C" int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    Auto_Lock_Scope local_lock(pthread_key_lock);
    for (unsigned i = 0; i < pthread_global.size(); ++i) {
        auto &p = pthread_global[i];
        if (not p.valid) {
            p.valid = true;
            p.destructor = destructor;
            *key = i;
            return 0;
        }
    }

    return -1; 
}


extern "C" void* pthread_getspecific(pthread_key_t key)
{
    return (void*)get_cpu_struct()->pthread_once_storage[key];
}

extern "C" int pthread_setspecific(pthread_key_t key, const void* data)
{
    get_cpu_struct()->pthread_once_storage[key] = data;

    return 0;
}

extern "C" int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    if (*once_control != 1) {
        bool r = __sync_bool_compare_and_swap(once_control, 0, 1);

        if (r) {
            init_routine();
        }
    }

    return 0;
}

extern "C" int strcmp(const char *str1, const char *str2)
{
    while (*str1) {
        if (*str1 != *str2)
            break;

        ++str1; ++str2;
    }

    return (int)*str1 - (int)*str2;
}

void copy_from_phys(u64 phys_addr, void* to, size_t size)
{
    Temp_Mapper_Obj<void> mapper(request_temp_mapper());
    for (u64 i = phys_addr&~0xfffULL; i < phys_addr+size; i += 0x1000) {
        mapper.map(i);
        const u64 start = i < phys_addr ? phys_addr : i;
        const u64 end = i+0x1000 < phys_addr+size ? i+0x1000 : phys_addr+size;
        memcpy((char*)to + (start-phys_addr), (char*)mapper.ptr + (start-i), end-start);
    }
}

size_t strlen_over_phys(u64 phys_addr)
{
    Temp_Mapper_Obj<char> mapper(request_temp_mapper());

    size_t len = 0;
    for (u64 i = phys_addr&~0xfffULL; ; i += 0x1000) {
        mapper.map(i);
        const u64 start = i < phys_addr ? phys_addr : i;
        const u64 end = i+0x1000;
        for (u64 j = start; j < end; ++j) {
            if (mapper.ptr[j-i] == '\0')
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