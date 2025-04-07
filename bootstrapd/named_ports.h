#pragma once
#include <pmos/ports.h>

// Registers the port with a given name
int register_port(const char *name, size_t name_length, pmos_port_t port);

// Requests the port. Calls the given callback after it was resolved
typedef void (*register_callback_t)(int result, const char *port_name, pmos_port_t port);

// Requests the given port. Calls the callback once it's done.
int request_port_callback(const char *name, size_t name_length, register_callback_t callback);

void request_port_message(const char *name, size_t length, int flags, pmos_port_t reply_port);