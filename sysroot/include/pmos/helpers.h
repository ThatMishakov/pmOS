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

#ifndef _PMOS_HELPERS_H
#define _PMOS_HELPERS_H

#include "containers/rbtree.h"
#include "system.h"

#include <kernel/messaging.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint64_t pmos_port_t;

#ifdef __STDC_HOSTED__

/**
 * @brief Blocks the process and upon the message reception, allocates memory with malloc(), and
 * fills it with the content. Internally, calls get_message_info() and get_first_message()
 *
 * @param desc [out] Pointer to the memory location where Message_descriptor will be filled
 * @param message [out] malloc'ed message. To avoid memory leaks, it should be freed when it's no longer
 * nedded
 * @param port [in] Valid port, to which the callee should be the owner, from where to get the message
 * @param reply_right [out] Reply right sent with the message. NULL discards it on reception.
 * @param other_rights [out] Array of 4 extra rights in the message. NULL discards them on reception.
 * @return result of the execution
 */
result_t get_message(Message_Descriptor *desc, unsigned char **message, pmos_port_t port, pmos_right_t *reply_right, pmos_right_t *other_rights);

/**
 * @brief Requests a timer message from the system. On success, the given port will receive a
 * IPC_Timer_Reply after at least ms milliseconds.
 *
 * @param port Port to receive the message
 * @param ms Time in milliseconds
 * @param extra parameter, that get saved returned in the IPC_Timer_Reply
 * @return int 0 on success, -1 on failure. Sets errno on error
 */
int pmos_request_timer(pmos_port_t port, uint64_t ms, uint64_t extra);

int pmos_msgloop_compare(uint64_t *a, uint64_t *b);
int pmos_msgloop_key_compare(uint64_t *a, uint64_t b);

void pmos_hexdump(FILE *stream, const char *data, size_t data_size);

struct pmos_msgloop_data;
typedef int (*pmos_msgloop_callback_t)(Message_Descriptor *desc, void *message,
                                       pmos_right_t *reply_right, pmos_right_t *extra_rights,
                                       void *ctx, struct pmos_msgloop_data *data);
struct msgloop_data {
    uint64_t right_id;
    pmos_msgloop_callback_t callback;
    void *ctx;
};

RBTREE(pmos_msgloop_tree, struct msgloop_data, pmos_msgloop_compare, pmos_msgloop_compare);
struct pmos_msgloop_data {
    pmos_msgloop_tree_tree_t nodes;
    pmos_port_t port;
    pmos_msgloop_tree_node_t *default_node;
};

void pmos_msgloop_initialize(struct pmos_msgloop_data *data, pmos_port_t port);
void pmos_msgloop_insert(struct pmos_msgloop_data *data, pmos_msgloop_tree_node_t *node);
pmos_msgloop_tree_node_t *pmos_msgloop_get(struct pmos_msgloop_data *data, pmos_right_t right);
int pmos_msgloop_erase(struct pmos_msgloop_data *data, pmos_msgloop_tree_node_t *node);
void pmos_msgloop_loop(struct pmos_msgloop_data *data);

void pmos_msgloop_node_set(pmos_msgloop_tree_node_t *n, pmos_right_t right_id,
                           pmos_msgloop_callback_t callback, void *ctx);

    #define PMOS_MSGLOOP_CONTINUE 0
    #define PMOS_MSGLOOP_BREAK    1

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif