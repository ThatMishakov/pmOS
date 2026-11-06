#include <types.hh>
#include <utils.hh>

extern "C" void unexpected_exception()
{
    panic("Unexpected exception!\n");
}

extern "C" void handle_user_sync()
{
    panic("Unhandlede handle_user_sync");
}

extern "C" void handle_user_irq()
{
    panic("Unhandled handle_user_irq");
}

extern "C" void handle_user_fiq()
{
    panic("Unhandled handle_user_fiq");
}

extern "C" void handle_user_serror()
{
    panic("Unhandled handle_user_serror");
}