#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/helpers.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test_delete_ipc_send_once()
{
    printf("Testing IPC deletion notification...\n");

    ports_request_t port_req = create_port(TASK_ID_SELF, 0);
    assert(port_req.result == SUCCESS);

    pmos_port_t port = port_req.port;
    pmos_right_t recieve_right;
    right_request_t right_req = create_right(port, &recieve_right, CREATE_RIGHT_SEND_ONCE);
    assert(right_req.result == SUCCESS);

    result_t delete_result = delete_right(right_req.right);
    assert(delete_result == SUCCESS);

    Message_Descriptor desc;
    uint8_t *msg;
    result_t get_result = get_message(&desc, &msg, port, NULL, NULL);
    assert(get_result == SUCCESS);

    assert(desc.size >= sizeof(IPC_Kernel_Recieve_Right_Destroyed_NUM));
    IPC_Kernel_Recieve_Right_Destroyed *notification = (IPC_Kernel_Recieve_Right_Destroyed *)msg;
    assert(notification->type == IPC_Kernel_Recieve_Right_Destroyed_NUM);
    assert(desc.sent_with_right == recieve_right);
    assert(desc.sender == 0);

    printf("IPC deletion notification received successfully.\n");
    free(msg);
    pmos_delete_port(port);
}

void test_delete_ipc_send_many()
{
    printf("Testing IPC deletion notification for send many...\n");

    ports_request_t port_req = create_port(TASK_ID_SELF, 0);
    assert(port_req.result == SUCCESS);

    pmos_port_t port = port_req.port;
    pmos_right_t recieve_right;
    right_request_t right_req = create_right(port, &recieve_right, 0);
    assert(right_req.result == SUCCESS);

    result_t delete_result = delete_right(right_req.right);
    assert(delete_result == SUCCESS);

    Message_Descriptor desc;
    uint8_t *msg;
    result_t get_result = get_message(&desc, &msg, port, NULL, NULL);
    assert(get_result == SUCCESS);

    assert(desc.size >= sizeof(IPC_Kernel_Recieve_Right_Destroyed_NUM));
    IPC_Kernel_Recieve_Right_Destroyed *notification = (IPC_Kernel_Recieve_Right_Destroyed *)msg;
    assert(notification->type == IPC_Kernel_Recieve_Right_Destroyed_NUM);
    assert(desc.sent_with_right == recieve_right);
    assert(desc.sender == 0);

    printf("IPC deletion notification for send many received successfully.\n");
    free(msg);
    pmos_delete_port(port);
}

void test_watch_right()
{
    printf("Testing watch right...\n");

    ports_request_t port_req = create_port(TASK_ID_SELF, 0);
    assert(port_req.result == SUCCESS);

    pmos_port_t port1 = port_req.port;
    pmos_right_t recieve_right;
    right_request_t right_req = create_right(port1, &recieve_right, 0);
    assert(right_req.result == SUCCESS);

    port_req = create_port(TASK_ID_SELF, 0);
    assert(port_req.result == SUCCESS);
    pmos_port_t port2 = port_req.port;

    right_request_t watch_result = watch_right(right_req.right, port2);
    assert(watch_result.result == SUCCESS);

    result_t delete_result = delete_right(right_req.right);
    assert(delete_result == SUCCESS);

    Message_Descriptor desc;
    uint8_t *msg;
    result_t get_result = get_message(&desc, &msg, port1, NULL, NULL);
    assert(get_result == SUCCESS);

    assert(desc.size >= sizeof(IPC_Kernel_Recieve_Right_Destroyed_NUM));
    assert(desc.sent_with_right == recieve_right);
    IPC_Kernel_Recieve_Right_Destroyed *notification = (IPC_Kernel_Recieve_Right_Destroyed *)msg;
    assert(notification->type == IPC_Kernel_Recieve_Right_Destroyed_NUM);
    assert(desc.sent_with_right == recieve_right);
    assert(desc.sender == 0);

    free(msg);
    get_result = get_message(&desc, &msg, port2, NULL, NULL);
    assert(get_result == SUCCESS);

    assert(desc.size >= sizeof(IPC_Kernel_Right_Destroyed_NUM));
    assert(desc.sent_with_right == watch_result.right);
    IPC_Kernel_Right_Destroyed *watch_notification = (IPC_Kernel_Right_Destroyed *)msg;
    assert(watch_notification->type == IPC_Kernel_Right_Destroyed_NUM);
    assert(watch_notification->right_type == 2); // SendMany
    assert(watch_notification->flags == 0);
    assert(desc.sender == 0);

    printf("Watch right notification received successfully.\n");
    free(msg);
    pmos_delete_port(port1);
    pmos_delete_port(port2);
}

void test_delete_ipc()
{
    test_delete_ipc_send_once();
    test_delete_ipc_send_many();
    test_watch_right();
}