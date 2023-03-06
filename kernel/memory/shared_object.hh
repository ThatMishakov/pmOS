#pragma once
#include <lib/memory.hh>


class Shared_Object: public klib::enable_shared_from_this<Shared_Object> {
public:
    static klib::shared_ptr<Shared_Object> create();

protected:

};