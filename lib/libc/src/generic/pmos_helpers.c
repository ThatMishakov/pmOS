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

#include <assert.h>
#include <errno.h>
#include <pmos/helpers.h>
#include <stdlib.h>

result_t get_message(Message_Descriptor *desc, unsigned char **message, pmos_port_t port,
                     pmos_right_t *reply_right, pmos_right_t *other_rights)
{
    result_t result = syscall_get_message_info(desc, port, 0);
    if (result != SUCCESS)
        return result;

    *message = malloc(desc->size);
    if (*message == NULL) {
        return -ENOMEM; // This needs to be changed
    }

    if (other_rights) {
        result_t result = accept_rights(port, other_rights);
        assert(result == SUCCESS);
    }

    if (reply_right) {
        right_request_t r = get_first_message((char *)*message, 0, port);
        *reply_right = r.right;
        result = r.result;
    } else {
        result = get_first_message((char *)*message, MSG_ARG_REJECT_RIGHT, port).result;
    }
    if (result != SUCCESS) {
        free(*message);
    }

    return result;
}