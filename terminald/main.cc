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
#include <pmos/helpers.hh>

#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>

struct flanterm_context *ft_ctx;

void write_screen(std::string_view s)
{
    flanterm_write(ft_ctx, s.data(), s.length());
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

pmos::Port main_port = pmos::Port::create().value();
pmos::Port configuration_port = pmos::Port::create().value();

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
    };

    auto loader_right = pmos::get_right_by_name("/pmos/loader").value();
    pmos::send_message_right_one(loader_right, req, {&configuration_port, pmos::RightType::SendOnce}).value();

    Message_Descriptor desc = {};
    unsigned char* message = NULL;
    auto result = get_message(&desc, &message, configuration_port.get(), nullptr, nullptr);
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

    mem_request_ret_t map_request = create_phys_map_region(TASK_ID_SELF, 0, size_alligned, PROT_READ|PROT_WRITE, start);
    if (map_request.result != SUCCESS)
        exit(7);

    framebuffer_ptr = (void*)((u64)map_request.virt_addr + (r->framebuffer_addr&0xfff));

    ft_ctx = flanterm_fb_init(malloc, flanterm_free, (uint32_t *)framebuffer_ptr, r->framebuffer_width, r->framebuffer_height, r->framebuffer_pitch,
        r->framebuffer_red_mask_size, r->framebuffer_red_mask_shift, r->framebuffer_green_mask_size, r->framebuffer_green_mask_shift, r->framebuffer_blue_mask_size, r->framebuffer_blue_mask_shift,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1, 1, 1, 0, 0, 0);

    if (ft_ctx == NULL)
        exit(8);

    free(message);
}

const char log_port_name[] = "/pmos/logd";
pmos::Right log_right;

void request_logger_port()
{
    // TODO: Add a syscall for this
    request_named_port(log_port_name, strlen(log_port_name), main_port.get(), 0);
}

void react_named_port_notification(char *msg_buff, size_t size, pmos::Right r)
{
    IPC_Kernel_Named_Port_Notification *msg = (IPC_Kernel_Named_Port_Notification *)msg_buff;
    if (size < sizeof(IPC_Kernel_Named_Port_Notification))
        return;
    
    log_right = std::move(r);

    IPC_Register_Log_Output reg = {
        .type = IPC_Register_Log_Output_NUM,
        .flags = 0,
        .task_id = get_task_id(),
    };

    auto terminal_right = main_port.create_right(pmos::RightType::SendMany).value();

    pmos::send_message_right_one(log_right, reg, {&main_port, pmos::RightType::SendOnce}, false, std::move(terminal_right.first));
}

int main() {
    ports_request_t req;

    init_screen();
    const char msg[] = "Initialized framebuffer...\n";
    flanterm_write(ft_ctx, msg, sizeof(msg));

    request_logger_port();

    while (1)
    {
        auto [msg, buffer, right, rights] = main_port.get_first_message().value();

        if (msg.size < sizeof(IPC_Write_Plain)-1) {
            write_screen("Warning: recieved very small message from ");
            print_hex(msg.sender);
            write_screen(" of size ");
            print_hex(msg.size);
            write_screen("\n");
            break;
        }

        IPC_Write_Plain *str = (IPC_Write_Plain *)(buffer.data());

        switch (str->type) {
        case IPC_Write_Plain_NUM:
            write_screen({str->data, msg.size - offsetof(IPC_Write_Plain, data)});
            break;
        case IPC_Kernel_Named_Port_Notification_NUM:
            react_named_port_notification((char *)buffer.data(), msg.size, std::move(rights[0]));
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
    }
    

    return 0;
}