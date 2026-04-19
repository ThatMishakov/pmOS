#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/helpers.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

void test_delete_ipc()
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
    pmos_delete_port(port);
}