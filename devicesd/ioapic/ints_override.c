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