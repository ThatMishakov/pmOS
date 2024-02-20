#include <args.h>
#include <utils.h>

void push_arg(Args_List_Header* header, const char* param)
{
    args_list_node* p;

    if (header->p == 0) {
        header->p = (args_list_node*)sizeof(Args_List_Header);
        p = (args_list_node*)((uint64_t)header + (uint64_t)header->p);        
    } else {
        p = (args_list_node*)((uint64_t)header + (uint64_t)header->p);
        while (p->next != 0) {
            p = (args_list_node*)((uint64_t)p + (uint64_t)p->next);
        }
        uint64_t size_alligned = (p->size & ~0b111) + ((p->size & 0b111) ? 0b1000 : 0);
        p->next = (args_list_node*)(sizeof(args_list_node) + size_alligned);
        p = (args_list_node*)((uint64_t)p + (uint64_t)p->next);
    }
    p->size = strlen(param);
    memcpy(param, (char*)((uint64_t)(p)+sizeof(args_list_node)), p->size);
    p->next = 0;
    ++header->size;
}