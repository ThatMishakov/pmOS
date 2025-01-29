#pragma once
#include <memory/temp_mapper.hh>

/**
 * @brief Temp_Mapper for PAE
 *
 */
class x86_PAE_Temp_Mapper final: public Temp_Mapper
{
public:
    virtual void *kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    constexpr x86_PAE_Temp_Mapper() = default;
    x86_PAE_Temp_Mapper(void *virt_addr, u32 cr3);

private:
    u64 *pt_mapped                 = nullptr;
    unsigned start_index           = 0;
    unsigned min_index             = 1;
    constexpr static unsigned size = 16;
};

class x86_2level_Mapper final: public Temp_Mapper
{
public:
    virtual void *kern_map(u64 phys_frame) override;
    virtual void return_map(void *) override;

    constexpr x86_2level_Mapper() = default;
    x86_2level_Mapper(void *virt_addr, u32 cr3);

private:
    u32 *pt_mapped                 = nullptr;
    unsigned start_index           = 0;
    unsigned min_index             = 1;
    constexpr static unsigned size = 16;

    static u32 temp_mapper_get_index(u32 addr);
};

Temp_Mapper *create_temp_mapper(void *virt_addr, u32 cr3);