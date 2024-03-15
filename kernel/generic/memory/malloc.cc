#include "malloc.hh"
#include <utils.hh>
#include "palloc.hh"
#include <lib/new.hh>
#include <kern_logger/kern_logger.hh>

size_t malloced = 0;
size_t freed = 0;

extern "C" void *mmap(void *, size_t length, int , int , int , long )
{
    // This function is (only) called by malloc
    // Only len is interesting
    void* p = palloc(length/4096 + (length%4096 ? 1 : 0));
    if (p == nullptr)
        return (void *)-1UL;

    return p;
}

extern "C" int munmap(void *addr, size_t length)
{
    (void)addr;
    (void)length;

    serial_logger.printf("Info: munmap(%p, %x)\n", addr, length);

    return -1;
    // Can be implemented easilly, but not supported for now
}
 
void *operator new(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}
 
void *operator new[](size_t size)
{
    void* ptr = malloc(size);
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}
 
void operator delete(void *p)
{
    free(p);
}
 
void operator delete[](void *p)
{
    free(p);
}

 void operator delete(void *p, UNUSED size_t s)
{
    free(p);
}
 
void operator delete[](void *p, UNUSED size_t s)
{
    free(p);
}

void *operator new(size_t, void *p)     noexcept { return p; }
void *operator new[](size_t, void *p)   noexcept { return p; }
void  operator delete  (void *, void *) noexcept { };
void  operator delete[](void *, void *) noexcept { };