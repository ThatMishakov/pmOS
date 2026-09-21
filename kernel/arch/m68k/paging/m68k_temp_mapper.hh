#pragma once
#include <memory/temp_mapper.hh>

namespace kernel::m68k::paging
{

struct M68K_Temp_Mapper : public kernel::paging::Temp_Mapper {
    virtual void *kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;
};

} // namespace kernel::m68k::paging