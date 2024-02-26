#include "malloc.hh"
#include <utils.hh>
#include "palloc.hh"
#include <lib/new.hh>

size_t malloced = 0;
size_t freed = 0;

extern "C" void *sbrk(size_t bytes)
{
    size_t size_pages = bytes/4096 + (bytes%4096 ? 1 : 0);
    void* p = palloc(size_pages);
    return p;
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