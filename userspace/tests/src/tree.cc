#include <pmos/containers/set.hh>
#include <ranges>
#include <cstdint>
#include <cstdio>

void test_set()
{
    printf("Testing set...\n");
    pmos::containers::set<uint64_t> set;

    for (uint64_t i = 0; i < 42; ++i)
        set.insert_noexcept(i);

    for (uint64_t i = 1; i < 42; ++i)
        set.erase(i);

    printf("Done!\n");
}

void test_containers()
{
    test_set();
}