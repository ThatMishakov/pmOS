#include <ioapic/ints_override.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct redir_list {
    struct redir_list* next;
    int_redirect_descriptor desc;
} redir_list;

redir_list* redir_list_head = NULL;

void register_redirect(uint32_t source, uint32_t to, uint8_t active_low, uint8_t level_trig)
{
    redir_list* node = malloc(sizeof(redir_list));

    node->desc.source = source;
    node->desc.destination = to;
    node->desc.active_low = active_low;
    node->desc.level_trig = level_trig;

    node->next = redir_list_head;
    redir_list_head = node;
}

int_redirect_descriptor get_for_int(uint32_t intno)
{
    redir_list* p = redir_list_head;

    while (p && p->desc.source != intno)
        p = p->next;

    if (p) {
        return p->desc;
    } else {
        int_redirect_descriptor desc = {intno, intno, 0, 0};
        return desc;
    }
}