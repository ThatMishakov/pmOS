#pragma once
#include <pmos/helpers.hh>

void init_pty_filesystem();

void openpt_handle(pmos::Right reply_right, unsigned oflags);