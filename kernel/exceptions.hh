#pragma once
#include <lib/string.hh>
#include "types.hh"

struct Kern_Exception {
    kresult_t err_code = 0;
    klib::string err_message;

	Kern_Exception(kresult_t err_code, klib::string msg = ""):
		err_code(err_code), err_message(klib::forward<klib::string>(msg)) {};
};