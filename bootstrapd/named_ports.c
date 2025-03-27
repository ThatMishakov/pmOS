#include <pmos/containers/rbtree.h>
#include <pmos/ports.h>
#include <string.h>

struct NamedPort {
    char *string;
    pmos_port_t port;
};

static int port_compare(struct NamedPort *a, struct NamedPort *b)
{
    return strcmp(&a, &b);
}

struct BoundedString {
    const char *str;
    size_t length;
};

int key_compare(struct NamedPort *a, struct BoundedString *s);

RBTREE(ports_tree, struct NamedPort, port_compare, key_compare)

