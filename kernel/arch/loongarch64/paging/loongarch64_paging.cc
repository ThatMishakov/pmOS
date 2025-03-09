#include "loongarch64_paging.hh"

#include <loongarch_asm.hh>
#include <memory/temp_mapper.hh>

constexpr u64 DIRECT_MAP_REGION = 0x8000000000000000;

class LoongArch64TempMapper final: public Temp_Mapper
{
public:
    virtual void *kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;
} mapper;

void *LoongArch64TempMapper::kern_map(u64 phys_frame) { return (void *)(phys_frame + DIRECT_MAP_REGION); }
void LoongArch64TempMapper::return_map(void *) {}

Temp_Mapper &CPU_Info::get_temp_mapper()
{
    return mapper;
}

void LoongArch64_Page_Table::apply() noexcept
{
    set_pgdl(page_directory);
    flush_tlb();
}
