#pragma once
#include <utils.hh>

namespace kernel::x86::tsc
{

extern FreqFraction tsc_freq;
extern FreqFraction tsc_inverted_freq;

void init_tsc();

bool use_tsc_deadline();

}