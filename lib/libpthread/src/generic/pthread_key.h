#ifndef PTHREAD_KEY_H
#define PTHREAD_KEY_H

// pthread_key.c implements the pthread_key_*, pthread_setspecific, and
// pthread_getspecific functions. The details are explained in <pthread.h>
// and implementation comments are in pthread_key.c


/// Internal function which is called upon thread exit to call the destructors
/// for all pthread_key_t's that have non-NULL values in the current thread.
void __pthread_key_call_destructors();

#endif // PTHREAD_KEY_H