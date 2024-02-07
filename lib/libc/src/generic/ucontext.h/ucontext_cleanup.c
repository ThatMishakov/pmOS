#include <ucontext.h>
#include <stdlib.h>

/// @brief Ucontext cleanup function
///
/// This is an internal function that gets called when ucontext is done
/// executing. It will set the context to the uc_link context if it is not
/// NULL. If it is NULL, it will exit the program.
/// @param uc_link The context to set to    
void __attribute__((visibility("hidden"))) __ucontext_cleanup(ucontext_t *uc_link) {
    if (uc_link == NULL)
        exit(0);

    setcontext(uc_link);
}