#include <unistd.h>
#include <stdio.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include <stdlib.h>

void timer_free_test()
{
    printf("Testing timer free...\n");
    pid_t pid = fork();
    if (pid != 0)
        return;

    ports_request_t req = create_port(TASK_ID_SELF, 0);
    if (req.result != SUCCESS) {
        printf("Error creating port %li\n", req.result);
        exit(1);
    }

    pmos_port_t port = req.port;

    right_request_t right_req = pmos_create_timer(port);
    if (right_req.result != SUCCESS) {
        printf("Error creating timer right %li\n", right_req.result);
        exit(1);
    }

    pmos_right_t timer_right = right_req.right;
    result_t arm_result = pmos_set_timer(port, timer_right, 10'000'000'000, PMOS_SET_TIMER_RELATIVE);
    if (arm_result != SUCCESS) {
        printf("Error arming timer %li\n", arm_result);
        exit(1);
    }

    exit(0);
}
