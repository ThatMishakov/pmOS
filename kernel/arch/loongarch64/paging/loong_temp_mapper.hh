#pragma once
#include <memory/temp_mapper.hh>

class LoongArch64TempMapper final: public Temp_Mapper
{
public:
    virtual void *kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;
};

extern LoongArch64TempMapper temp_mapper;

void set_dmws();