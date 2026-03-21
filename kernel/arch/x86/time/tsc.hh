#pragma once

namespace kernel::x86::tsc
{

void init_tsc();

bool use_tsc_deadline();

}