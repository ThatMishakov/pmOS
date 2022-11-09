#include <kernel/types.h>
#include <stdlib.h>
#include <string.h>
#include <internal_param.h>
#include <system.h>

typedef struct {
    int argc;
    char **argv;
} Args_Ret;

Args_Ret prepare_args(u64 argtype, u64 ptr)
{
    Args_Ret p = {0,0};

    switch (argtype) {
    case ARGS_LIST: {
        Args_List_Header* args_list = (Args_List_Header*)ptr;
        p.argc = args_list->size;
        p.argv = (char**)malloc(sizeof(char*) * args_list->size);

        args_list_node* n = (args_list_node*)((u64)(args_list->p) + ptr);
        for (u64 i = 0; i < args_list->size; ++i) {
            p.argv[i] = malloc(n->size + 1);
            p.argv[i][n->size] = '\0';

            const char* source = ((const char*)n) + sizeof(args_list_node);
            p.argv[i] = strncpy(p.argv[i], source, n->size);

            n = (args_list_node*)((u64)n->next + ptr);
        }

        if (args_list->flags & ARGS_LIST_FLAG_ASTEMPPAGE) {
            syscall_release_page_multi(ptr, args_list->pages);
        }
    }
        break;
    default:
        // Not supported or malformed
        break;
    };

    return p;
}