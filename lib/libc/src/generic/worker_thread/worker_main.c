#include <pthread.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include <pmos/ipc.h>
#include <stdbool.h>
#include <stdio.h>
#include <alloca.h>
#include <pmos/tls.h>
#include <string.h>
#include <pmos/__internal.h>

pmos_port_t worker_port;
uint64_t worker_port_right;
__attribute__((noreturn)) void _syscall_exit(int status);

void *main_trampoline(void *);

struct uthread *worker_thread;

void __close_files_on_exit();
void __terminate_threads();

static const char *processd_port_name = "/pmos/processd";
static pmos_port_t processd_port = 0;

pmos_port_t __get_processd_port()
{
    if (processd_port != 0) {
        return processd_port;
    }

    ports_request_t port_req =
        get_port_by_name(processd_port_name, strlen(processd_port_name), 0);
    if (port_req.result != SUCCESS) {
        // Handle error
        return INVALID_PORT;
    }

    return port_req.port;
}

uint64_t process_task_group = 0;
bool registering_process = false;

pid_t _HIDDEN __pid_cached = 0;

uint64_t pmos_process_task_group()
{
    return process_task_group;
}

int __register_process()
{
    IPC_Register_Process msg = {
        .type = IPC_Register_Process_NUM,
        .flags = 0,
        .task_group_id = process_task_group, 
        .reply_port = worker_port,
        .signal_port = worker_port,
        .worker_task_id = TASK_ID_SELF,
    };

    result_t r = send_message_port(processd_port, sizeof(msg), &msg);
    if (r != SUCCESS) {
        return -1;
    }

    registering_process = true;

    return 0;
}

void __do_fork(uint64_t requester, pmos_port_t reply_port);

void worker_main()
{
    worker_thread = __get_tls();

    syscall_r sys_result = create_task_group();
    if (sys_result.result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Failed to create task group\n");
        _syscall_exit(1);
    }
    process_task_group = sys_result.value;

    sys_result = set_namespace(process_task_group, NAMESPACE_RIGHTS);
    if (sys_result.result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Failed to set task group namespace\n");
        _syscall_exit(1);
    }

    ports_request_t new_port = create_port(TASK_ID_SELF, 0);
    if (new_port.result != SUCCESS) {
        fprintf(stderr, "pmOS libC: Failed to initialize worker port\n");
        _syscall_exit(1);
    }
    worker_port = new_port.port;

    uint64_t reciever_port;
    sys_result = create_right(worker_port, &reciever_port, 0);
    if (sys_result.result != SUCCESS) {
        fprintf(stderr, "Failed to create the task group right\n");
        _syscall_exit(1);
    }

    // result_t r = request_named_port(processd_port_name, strlen(processd_port_name), worker_port, 0);
    // if (r != SUCCESS) {
    //     fprintf(stderr, "pmOS libC: Failed to request processd port. Result: %li\n", r);
    //     _syscall_exit(1);
    // }

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

        void *msg_buff = alloca(msg.size);

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
    case IPC_Request_Fork_NUM: {
        IPC_Request_Fork *fork_msg = (IPC_Request_Fork *)ipc_msg;
        __do_fork(msg.sender, fork_msg->reply_port);
        break;
    }
    case IPC_Kernel_Named_Port_Notification_NUM: {
        IPC_Kernel_Named_Port_Notification *notification = (IPC_Kernel_Named_Port_Notification *)ipc_msg;
        size_t len = msg.size - sizeof(IPC_Kernel_Named_Port_Notification);
        if (len == strlen(processd_port_name) && strncmp(notification->port_name, processd_port_name, len) == 0) {
            processd_port = notification->port_num;
            if (__register_process() < 0) {
                fprintf(stderr, "pmOS libC: Failed to register process\n");
                running = false;
            }
        }
        break;
    }
    case IPC_Register_Process_Reply_NUM: {
        IPC_Register_Process_Reply *reply = (IPC_Register_Process_Reply *)ipc_msg;
        if (reply->result != 0) {
            fprintf(stderr, "pmOS libC: Failed to register process\n");
            running = false;
        }
        registering_process = false;
        __pid_cached = reply->pid;
        break;
    }
    default:
        // TODO: Handle messages
        printf("pmOS libC: Received message %i from task 0x%lx PID (cached) %li\n", ipc_msg->type, msg.sender, __pid_cached);
        break;
        }
    }

    if (!abnormal_termination)
        __close_files_on_exit();
        
    __terminate_threads();
    _syscall_exit((int)(long)exit_code);
}