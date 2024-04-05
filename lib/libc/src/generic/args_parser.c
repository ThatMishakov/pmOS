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

#include <kernel/types.h>
#include <stdlib.h>
#include <string.h>
#include <internal_param.h>
#include <pmos/system.h>
#include <string.h>
#include <stdio.h>

char buff[200];

typedef struct {
    int argc;
    char **argv;
} Args_Ret;

Args_Ret prepare_args(u64 argtype, u64 ptr)
{
    Args_Ret p = {0,0};

    // TODO: Needs fixing after completely changing memory syscalls

    // switch (argtype) {
    // case ARGS_LIST: {
    //     Args_List_Header* args_list = (Args_List_Header*)ptr;
    //     p.argc = args_list->size;
    //     p.argv = (char**)malloc(sizeof(char*) * args_list->size);

    //     args_list_node* n = (args_list_node*)((u64)(args_list->p) + ptr);
    //     for (u64 i = 0; i < args_list->size; ++i) {
    //         p.argv[i] = malloc(n->size + 1);

    //         p.argv[i][n->size] = '\0';
    //         const char* source = ((const char*)n) + sizeof(args_list_node);
    //         p.argv[i] = strncpy(p.argv[i], source, n->size);

    //         n = (args_list_node*)((u64)n->next + (u64)n);
    //     }

    //     if (args_list->flags & ARGS_LIST_FLAG_ASTEMPPAGE) {
    //         syscall_release_page_multi(ptr, args_list->pages);
    //     }
    // }
    //     break;
    // default:
    //     // Not supported or malformed
    //     break;
    // };

    return p;
}