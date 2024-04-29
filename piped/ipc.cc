#include "ipc.hh"
#include <system_error>

pmos_port_t create_port()
{
    ports_request_t request = create_port(TASK_ID_SELF, 0);
    if (request.result != SUCCESS)
        throw std::system_error(-request.result, std::system_category());
    return request.port;
}