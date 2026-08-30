#pragma once
#include <stddef.h>
#include "intrusive_list.hh"

namespace pmos::containers
{

template<typename Type, DoubleListHead<Type> Type:: *Head, size_t BucketCount, class Hash>
class HashMap
{
public:

    void insert(Type *item) noexcept;
    void remove(Type *item) noexcept;

    Type *find(const Type &item) noexcept;

private:
    struct Bucket {
        CircularDoubleList<Type, Head> list;
    };

    Bucket buckets[BucketCount];
    Hash hash_func;
};

template<typename Type, DoubleListHead<Type> Type:: *Head, size_t BucketCount, class Hash>
void HashMap<Type, Head, BucketCount, Hash>::insert(Type *item) noexcept
{
    size_t bucket_index = hash_func(*item) % BucketCount;
    buckets[bucket_index].list.push_back(item);
}

template<typename Type, DoubleListHead<Type> Type:: *Head, size_t BucketCount, class Hash>
void HashMap<Type, Head, BucketCount, Hash>::remove(Type *item) noexcept
{
    size_t bucket_index = hash_func(*item) % BucketCount;
    buckets[bucket_index].list.remove(item);
}

template<typename Type, DoubleListHead<Type> Type:: *Head, size_t BucketCount, class Hash>
Type *HashMap<Type, Head, BucketCount, Hash>::find(const Type &item) noexcept
{
    size_t bucket_index = hash_func(item) % BucketCount;
    auto &bucket = buckets[bucket_index];
    for (auto it = bucket.list.begin(); it != bucket.list.end(); ++it) {
        if (*it == item) {
            return &*it;
        }
    }
    return nullptr;
}

} // namespace pmos::containers