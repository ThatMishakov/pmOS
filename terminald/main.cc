#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
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
    //flanterm_write(ft_ctx, msg, strlen(msg));
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
        exit(r->result_code);

    // Map memory
    size_t start = r->framebuffer_addr&~0xfffUL;
    size_t size = r->framebuffer_width*r->framebuffer_height*r->framebuffer_bpp/8;
    size_t end = (start + size * 0xfff)&~0xfffUL;
    size_t size_alligned = end - start;

    mem_request_ret_t map_request = create_phys_map_region(PID_SELF, 0, size_alligned, PROT_READ|PROT_WRITE, (void*)start);
    if (map_request.result != SUCCESS)
        exit(7);

    framebuffer_ptr = (void*)((u64)map_request.virt_addr + (r->framebuffer_addr&0xfff));

    ft_ctx = flanterm_fb_simple_init(
        (uint32_t *)framebuffer_ptr, r->framebuffer_width, r->framebuffer_height, r->framebuffer_pitch
    );

    free(message);
}

int main() {
    ports_request_t req;
    req = create_port(PID_SELF, 0);
    if (req.result != SUCCESS) {
        write_screen("Error creating configuration port ");
        print_hex(req.result);
        write_screen("\n");
    }
    configuration_port = req.port;

    init_screen();
    const char msg[] = "Hello world\n";
    flanterm_write(ft_ctx, msg, sizeof(msg));
    write_screen(const_cast<char*>("Hello from terminald!\n"));

    req = create_port(PID_SELF, 0);
    if (req.result != SUCCESS) {
        write_screen("Error creating main port ");
        print_hex(req.result);
        write_screen("\n");
    }
    main_port = req.port;


    set_log_port(main_port, 0);

    {
        result_t r = name_port(main_port, terminal_port_name, strlen(terminal_port_name), 0);
        if (r != SUCCESS) {
            write_screen("terminald: Error 0x");
            print_hex(r);
            write_screen(" naming port\n");
        }

        r = name_port(main_port, stdout_port_name, strlen(stdout_port_name), 0);
        if (r != SUCCESS) {
            write_screen("terminald: Error 0x");
            print_hex(r);
            write_screen(" naming port\n");
        }

        r = name_port(main_port, stderr_port_name, strlen(stderr_port_name), 0);
        if (r != SUCCESS) {
            write_screen("terminald: Error 0x");
            print_hex(r);
            write_screen(" naming port\n");
        }
    }

    while (1)
    {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        char* msg_buff = (char*)malloc(msg.size+1);

        get_first_message(msg_buff, 0, main_port);

        msg_buff[msg.size] = '\0';

        if (msg.size < sizeof(IPC_Write_Plain)-1) {
            write_screen("Warning: recieved very small message\n");
            free(msg_buff);
            break;
        }

        IPC_Write_Plain *str = (IPC_Write_Plain *)(msg_buff);

        switch (str->type) {
        case IPC_Write_Plain_NUM:
            write_screen(str->data);
            break;
        default:
            write_screen("Warning: Unknown message type ");
            print_hex(str->type);
            write_screen("\n");
        }

        free(msg_buff);
    }
    

    return 0;
}