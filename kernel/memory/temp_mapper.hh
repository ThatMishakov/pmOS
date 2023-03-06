#pragma once
#include <types.hh>
#include "kernel/memory.h"

class Temp_Mapper {
public:
    // Maps page frame to the temporary location
    virtual void * kern_map(u64 phys_frame) = 0;
    
    // Returns the mapping
    virtual void return_map(void *) = 0;

    Temp_Mapper() = default;
    Temp_Mapper(const Temp_Mapper&) = delete;
};

template<typename P = void>
struct Temp_Mapper_Obj {
    P * ptr = nullptr;
    Temp_Mapper& parent;

    P * map(u64 phys_frame)
    {
        if (ptr != nullptr)
            parent.return_map(reinterpret_cast<void *>(ptr));

        ptr = reinterpret_cast<P *>(parent.kern_map(phys_frame));
        return ptr;
    }

    void clear()
    {
        if (ptr != nullptr)
            parent.return_map(reinterpret_cast<void *>(ptr));
    }



    ~Temp_Mapper_Obj()
    {
        clear();
    }

    constexpr Temp_Mapper_Obj(Temp_Mapper& t): ptr(nullptr), parent(t) {};

    Temp_Mapper_Obj(Temp_Mapper_Obj&& p): ptr(p.ptr), parent(p.parent)
    {
        p.ptr = nullptr;
    }

    Temp_Mapper_Obj(const Temp_Mapper_Obj&) = delete;
};


class x86_PAE_Temp_Mapper: public Temp_Mapper {
public:
    virtual void * kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    x86_PAE_Temp_Mapper();
private:
    PTE * pt_mapped = nullptr;
    unsigned max_index = 0;
    unsigned start_index = 0;
    unsigned min_index   = 1;
    constexpr static unsigned size = 16;
};