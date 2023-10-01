#ifndef TASK_LIST_H
#define TASK_LIST_H
#include <stdint.h>
#include <stdbool.h>
#include <multiboot2.h>

struct task_list_node {
    struct task_list_node *next;
    struct multiboot_tag_module * mod_ptr;
    char * name;
    char * path;
    char * cmdline;
    void * file_virt_addr;
    uint64_t page_table;
    void * tls_virt;
    bool executable;
    uint64_t stack_top;
    uint64_t stack_size;
    uint64_t stack_guard_size;
    uint64_t load_data_virt_addr;
    uint64_t load_data_size;
};

extern struct task_list_node *modules_list;

#endif // TASK_LIST_H