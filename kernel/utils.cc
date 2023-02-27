#include "utils.hh"
#include <stdarg.h>
#include "types.hh"
#include "asm.hh"
#include "start.hh"
#include <kernel/errors.h>
#include <memory/paging.hh>
#include <messaging/messaging.hh>
#include <memory/free_page_alloc.hh>
#include <lib/string.hh>
#include <kern_logger/kern_logger.hh>
#include <sched/sched.hh>

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

void t_write_bochs(const char * str, u64 length)
{
    for (u64 i = 0; i < length; ++i) {
        printc(str[i]);
    }
}

void t_print_bochs(const char *str, ...)
{
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

kresult_t prepare_user_buff_rd(const char* buff, size_t size)
{
    u64 addr_start = (u64)buff;
    u64 end = addr_start+size;

    if (addr_start > KERNEL_ADDR_SPACE or end > KERNEL_ADDR_SPACE or addr_start > end) return ERROR_OUT_OF_RANGE;

    kresult_t result = SUCCESS;

    for (u64 i = addr_start; i < end and result == SUCCESS; ++i) {
        u64 page = i & ~0xfffULL;
        result = get_cpu_struct()->current_task->page_table->prepare_user_page(page, Page_Table::Readable, get_cpu_struct()->current_task);
    }
    return result;
}

kresult_t prepare_user_buff_wr(char* buff, size_t size)
{
    // TODO: Fix read-only pages & stuff

    u64 addr_start = (u64)buff;
    u64 end = addr_start+size;

    if (addr_start > KERNEL_ADDR_SPACE or end > KERNEL_ADDR_SPACE or addr_start > end) return ERROR_OUT_OF_RANGE;

    kresult_t result = SUCCESS;

    for (u64 i = addr_start; i < end and result == SUCCESS; ++i) {
        u64 page = i & ~0xfffULL;
        result = get_cpu_struct()->current_task->page_table->prepare_user_page(page, Page_Table::Writeable, get_cpu_struct()->current_task);
    }
    return result;
}

kresult_t copy_from_user(char* to, const char* from, size_t size)
{
    kresult_t result = prepare_user_buff_rd(from, size);
    if (result != SUCCESS) return result;

    memcpy(to, from, size);

    return SUCCESS;
}

kresult_t copy_to_user(const char* from, char* to, size_t size)
{
    kresult_t result = prepare_user_buff_wr(to, size);
    if (result != SUCCESS) return result;

    memcpy(to, from, size);

    return SUCCESS;
}

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


Spinlock copy_frame_s;
Free_Page_Alloc_Static<2> copy_frame_free_p;

void copy_frame(u64 from, u64 to)
{
    copy_frame_s.lock();
    u64 t1 = copy_frame_free_p.get_free_page();
    u64 t2 = copy_frame_free_p.get_free_page();


    Page_Table_Argumments pta = {1, 0, 0, 1, 0b010};
    map(from, t1, pta);
    map(to, t2, pta);

    memcpy((char*)t1, (char*)t2, 4096);

    release_page_s(*get_pte(t1, rec_map_index));
    release_page_s(*get_pte(t2, rec_map_index));

    invalidade(t1);
    invalidade(t2);

    copy_frame_free_p.release_free_page(t1);
    copy_frame_free_p.release_free_page(t2);
    copy_frame_s.unlock();
}

struct stack_frame {
    stack_frame* next;
    void* return_addr;
};

void print_stack_trace()
{
    t_print_bochs("Stack trace:\n");
    struct stack_frame* s;
    __asm__ volatile("movq %%rbp, %0": "=a" (s));
    for (; s != NULL; s = s->next)
        t_print_bochs("  -> %h\n", (u64)s->return_addr);
}

extern "C" void abort(void)
{
    t_print_bochs("Error: abort() was called. Freezing...\n");
    print_stack_trace();
    while (1);
}

extern "C" void _assert_fail(const char* condition, const char* file, unsigned int line)
{
    t_print_bochs("Assert fail %s in file %s at line %i\n", condition, file, line);
    abort();
}