#include <pthread.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include <pmos/ipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <alloca.h>
#include <pmos/tls.h>

pmos_port_t worker_port;
__attribute__((noreturn)) void _syscall_exit(int status);

void *main_trampoline(void *);

struct uthread *worker_thread;

void __close_files_on_exit();
void __terminate_threads();

void worker_main()
{
    worker_thread = __get_tls();

    ports_request_t new_port = create_port(TASK_ID_SELF, 0);
    if (new_port.result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Failed to initialize worker port\n");
        _syscall_exit(1);
    }
    worker_port = new_port.port;

    // Run main function
    pthread_t main_thread;
    int result = pthread_create(&main_thread, NULL, main_trampoline, NULL);
    if (result != 0) {
        fprintf(stderr, "pmOS libC: Failed to create main thread from worker\n");
        _syscall_exit(1);
    }
    pthread_detach(main_thread);

    void *exit_code = NULL;
    bool running = true;
    bool abnormal_termination = false;
    while (running) {
        Message_Descriptor msg;
        result_t r = syscall_get_message_info(&msg, worker_port, 0);
        if (r != SUCCESS) {
            fprintf(stderr, "pmOS libC: Failed to get message info: %li\n", r);
            running = false;
            continue;
        }

        char *msg_buff = (char *)alloca(msg.size);

        r = get_first_message(msg_buff, 0, worker_port);
        if (r != SUCCESS) {
            fprintf(stderr, "pmOS libC: Failed to get message: %li\n", r);
            running = false;
            continue;
        }

        IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)msg_buff;

        switch (ipc_msg->type) {
            case IPC_Exit_NUM: {
                IPC_Exit *exit_msg = (IPC_Exit *)ipc_msg;
                if (exit_msg->exit_type == IPC_EXIT_TYPE_NORMAL || exit_msg->exit_type == IPC_EXIT_TYPE_ABNORMAL) {
                    exit_code = exit_msg->exit_code;
                    abnormal_termination = exit_msg->exit_type == IPC_EXIT_TYPE_ABNORMAL;
                    running = false;
                }
                break;
            }
            default:
                // TODO: Handle messages
                printf("pmOS libC: Received message %i from task 0x%lx\n", ipc_msg->type, msg.sender);
                break;
        }
    }

    if (!abnormal_termination)
        __close_files_on_exit();
        
    __terminate_threads();
    _syscall_exit((int)(long)exit_code);
}