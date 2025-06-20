#pragma once
#include <pmos/ports.h>
#include <pmos/system.h>

// Registers the port with a given name
int register_right(const char *name, size_t name_length, pmos_right_t right);

// Requests the port. Calls the given callback after it was resolved
typedef void (*register_callback_t)(int result, const char *port_name, pmos_right_t right);

// Requests the given port. Calls the callback once it's done.
int request_port_callback(const char *name, size_t name_length, register_callback_t callback);

void request_port_message(const char *name, size_t length, int flags, pmos_right_t reply_right);