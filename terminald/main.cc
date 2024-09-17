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

#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/block.h>
#include <string.h>
#include <pmos/system.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/memory.h>
#include <pmos/helpers.h>

#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>

struct flanterm_context *ft_ctx;

void write_screen(const char * msg)
{
    flanterm_write(ft_ctx, msg, strlen(msg));
}

void int_to_hex(char * buffer, uint64_t n, char upper)
{
  char buffer2[24];
  int buffer_pos = 0;
  do {
    char c = n & 0x0f;
    if (c > 9) c = c - 10 + (upper ? 'A' : 'a');
    else c = c + '0';
    buffer2[buffer_pos] = c;
    n >>= 4;
    ++buffer_pos;
  } while (n > 0);

  for (int i = 0; i < buffer_pos; ++i) {
    buffer[i] = buffer2[buffer_pos - 1 - i];
  }
  buffer[buffer_pos] = '\0';
}

void print_hex(uint64_t i)
{
    char buffer[24];
    write_screen(const_cast<char*>("0x"));
    int_to_hex(buffer, i, 1);
    write_screen(buffer);
}

pmos_port_t main_port = 0;
pmos_port_t configuration_port = 0;

const char terminal_port_name[] = "/pmos/terminald";
const char stdout_port_name[] = "/pmos/stdout";
const char stderr_port_name[] = "/pmos/stderr";

void *framebuffer_ptr = nullptr;


void flanterm_free(void *ptr, size_t)
{
    free(ptr);
}

void init_screen()
{
    IPC_Framebuffer_Request req = {
        .type = IPC_Framebuffer_Request_NUM,
        .flags = 0,
        .reply_port = configuration_port,
    };

    static const char* loader_port_name = "/pmos/loader";
    ports_request_t loader_port_req = get_port_by_name(loader_port_name, strlen(loader_port_name), 0);
    if (loader_port_req.result != SUCCESS)
        exit(1);

    result_t result = send_message_port(loader_port_req.port, sizeof(req), &req);
    if (result != SUCCESS)
        exit(2);

    Message_Descriptor desc = {};
    unsigned char* message = NULL;
    result = get_message(&desc, &message, configuration_port);
    if (result != SUCCESS)
        exit(3);

    if (desc.size < sizeof(IPC_Framebuffer_Reply))
        exit(4);

    IPC_Framebuffer_Reply *r = (IPC_Framebuffer_Reply*)message;
    if (r->type != IPC_Framebuffer_Reply_NUM)
        exit(5);

    if (r->result_code != 0)
        exit(-r->result_code);

    // Map memory
    size_t start = r->framebuffer_addr&~0xfffUL;
    size_t size = r->framebuffer_width*r->framebuffer_height*r->framebuffer_bpp/8;
    size_t end = (start + size + 0xfff)&~0xfffUL;
    size_t size_alligned = end - start;

    mem_request_ret_t map_request = create_phys_map_region(TASK_ID_SELF, 0, size_alligned, PROT_READ|PROT_WRITE, (void*)start);
    if (map_request.result != SUCCESS)
        exit(7);

    framebuffer_ptr = (void*)((u64)map_request.virt_addr + (r->framebuffer_addr&0xfff));

    ft_ctx = flanterm_fb_init(malloc, flanterm_free, (uint32_t *)framebuffer_ptr, r->framebuffer_width, r->framebuffer_height, r->framebuffer_pitch,
        r->framebuffer_red_mask_size, r->framebuffer_red_mask_shift, r->framebuffer_green_mask_size, r->framebuffer_green_mask_shift, r->framebuffer_blue_mask_size, r->framebuffer_blue_mask_shift,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1, 1, 1, 1, 1, 0);

    free(message);
}

const char log_port_name[] = "/pmos/logd";
pmos_port_t log_port = 0;

void request_logger_port()
{
    // TODO: Add a syscall for this
    pmos_syscall(SYSCALL_REQUEST_NAMED_PORT, log_port_name, strlen(log_port_name), main_port, 0);
}

void react_named_port_notification(char *msg_buff, size_t size)
{
    IPC_Kernel_Named_Port_Notification *msg = (IPC_Kernel_Named_Port_Notification *)msg_buff;
    if (size < sizeof(IPC_Kernel_Named_Port_Notification))
        return;
    
    log_port = msg->port_num;

    IPC_Register_Log_Output reg = {
        .type = IPC_Register_Log_Output_NUM,
        .flags = 0,
        .reply_port = main_port,
        .log_port = main_port,
        .task_id = get_task_id(),
    };

    send_message_port(log_port, sizeof(reg), &reg);
}

int main() {
    ports_request_t req;
    req = create_port(TASK_ID_SELF, 0);
    if (req.result != SUCCESS) {
        write_screen("Error creating configuration port ");
        print_hex(req.result);
        write_screen("\n");
    }
    configuration_port = req.port;

    init_screen();
    const char msg[] = "Initialized framebuffer...\n";
    flanterm_write(ft_ctx, msg, sizeof(msg));

    req = create_port(TASK_ID_SELF, 0);
    if (req.result != SUCCESS) {
        write_screen("Error creating main port ");
        print_hex(req.result);
        write_screen("\n");
    }
    main_port = req.port;

    request_logger_port();

    while (1)
    {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        char* msg_buff = (char*)malloc(msg.size+1);

        get_first_message(msg_buff, 0, main_port);

        msg_buff[msg.size] = '\0';

        if (msg.size < sizeof(IPC_Write_Plain)-1) {
            write_screen("Warning: recieved very small message from ");
            print_hex(msg.sender);
            write_screen(" of size ");
            print_hex(msg.size);
            write_screen("\n");
            free(msg_buff);
            break;
        }

        IPC_Write_Plain *str = (IPC_Write_Plain *)(msg_buff);

        switch (str->type) {
        case IPC_Write_Plain_NUM:
            write_screen(str->data);
            break;
        case IPC_Kernel_Named_Port_Notification_NUM:
            react_named_port_notification(msg_buff, msg.size);
            break;
        case IPC_Log_Output_Reply_NUM:
            // Ignore it :)
            // (TODO)
            break;
        default:
            write_screen("!!!!!!!!!!!!!!!!!!!\n");
            write_screen("Warning: Unknown message type ");
            print_hex(str->type);
            write_screen("\n");
        }

        free(msg_buff);
    }
    

    return 0;
}