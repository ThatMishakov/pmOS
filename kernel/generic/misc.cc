#include "misc.hh"
#include "linker.hh"
#include "utils.hh"

void* unoccupied = (void*)&_free_after_kernel;

extern "C" char    *__cxa_demangle_gnu3(const char * name)
{
    return (char*)name;
}