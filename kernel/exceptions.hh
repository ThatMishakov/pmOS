#pragma once
#include <lib/string.hh>
#include "types.hh"

struct Kern_Exception {
    kresult_t err_code = 0;
    const char * err_message = "";

	Kern_Exception(kresult_t err_code, const char * msg = ""):
		err_code(err_code), err_message(msg) {};
};