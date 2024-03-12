#pragma once
#include "exception.hh"
#include "string.hh"

namespace std {
  class logic_error : public exception {
    klib::string what_str;
  public:
    explicit logic_error(const klib::string& what_arg): what_str(what_arg) {};
    explicit logic_error(const char* what_arg): what_str(what_arg) {};

    const char* what() const noexcept
    {
        return what_str.c_str();
    }
  };
}

namespace std {
  class out_of_range : public logic_error {
  public:
    explicit out_of_range(const klib::string& what_arg):
        logic_error(what_arg) {};
    explicit out_of_range(const char* what_arg):
        logic_error(what_arg) {};
  };
}