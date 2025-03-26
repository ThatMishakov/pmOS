#include <pmos/containters/rbtree.h>
#include <pmos/ports.h>

struct NamedPort {
    char *string;
    pmos_port_t port;
};

static int port_compare(NamedPort &a, NamedPort &b)
{
    return strcmp(&a, &b);
}

RBTREE(ports_tree, NamedPort, port_compare)

