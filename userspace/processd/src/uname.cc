#include "uname.hh"

utsname name = {
    .sysname = "pmos",
    .nodename = "pmos",
    .release = "0",
    .version = "0",
    .machine = "pc",
    .domainname = "localhost",
};

utsname get_uname()
{
    return name;
}