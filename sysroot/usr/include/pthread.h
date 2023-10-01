#ifndef _PTHREAD_H
#define _PTHREAD_H 1

#define __DECLARE_SIZE_T
// POSIX doesn't seem to mention NULL here, but it's pulled from
// time.h and everyone expects it
#define __DECLARE_NULL
#include "__posix_types.h"

#include <time.h>
#include <sched.h>


#include "sys/types.h"

#define PTHREAD_CANCEL_ENABLE       (0x01)
#define PTHREAD_CANCEL_DISABLE      (0x02)

#define PTHREAD_CANCEL_DEFERRED     (0x01)
#define PTHREAD_CANCEL_ASYNCHRONOUS (0x02)

#define PTHREAD_CANCELLED (-1)

#define PTHREAD_COND_INITIALIZER ((pthread_cond_t)(NULL))

#define PTHREAD_CREATE_JOINABLE (0x00)
#define PTHREAD_CREATE_DETACHED (0x01)

#define PTHREAD_INHERIT_SCHED (0x01)
#define PTHREAD_EXPLICIT_SCHED (0x02)

#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t){0x00})
#define PTHREAD_ONCE_INIT ((pthread_once_t)(0x00))

#define PTHREAD_MUTEX_NORMAL (0x01)
#define PTHREAD_MUTEX_ERRORCHECK (0x02)
#define PTHREAD_MUTEX_RECURSIVE (0x03)
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_PRIO_NONE (0x01)
#define PTHREAD_PRIO_INHERIT (0x02)
#define PTHREAD_PRIO_PROTECT (0x03)

#define PTHREAD_PROCESS_PRIVATE (0x01)
#define PTHREAD_PROCESS_SHARED (0x02)

#define PTHREAD_RWLOCK_INITIALIZER { \
    PTHREAD_COND_INITIALIZER, \
    PTHREAD_MUTEX_INITIALIZER, \
    \
    0, \
    0, \
    0, \
}

#define PTHREAD_SCOPE_PROCESS (0x01);
#define PTHREAD_SCOPE_SYSTEM  (0x02);

#define PTHREAD_CREATE_JOINABLE (0)
#define PTHREAD_CREATE_DETACHED (1)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Uninitializes the pthread_attr_t structure
 * 
 * This function should uninitialize provided pthread_attr_t structure. In pmOS this does nothing and should
 * always return success, however you should call this function anyways for portability of your program.
 * 
 * @param attrs_ptr structure to be uninitialized.
 * @return 0 is returned upon the successfull execution. Otherwise, -1 is returned and errno is set to the error code.
 */
int   pthread_attr_destroy(pthread_attr_t * attrs_ptr);

/**
 * @brief Gets the detachstate attribute
 * 
 * _detachstate_ atribute controls whether the thread is created in a detached state. 
 * 
 * @param attrs_ptr Pointer to the pthread_attr_t holding the attribute.
 * @param detachstate Sets the pointer to the detachstate attrribute value defined in attrs_ptr. Possible values are:
 *                    PTHREAD_CREATE_DETACHED - new threads created with attr are in the detached state
 *                    PTHREAD_CREATE_JOINABLE - new threads created with attr are in the joinable state
 * @return 0 is returned upon the successfull execution. Otherwise -1 is returned and errno is set to the error code.
 */
int   pthread_attr_getdetachstate(const pthread_attr_t * attrs_ptr, int * detachstate);

/**
 * @brief Sets the detachstate attribute
 * 
 * _detachstate_ atribute controls whether the thread is created in a detached state. 
 * 
 * @param attrs_ptr Pointer to the pthread_attr_t which attribute should be set.
 * @param detachstate The new value of the pointer. Possible parameters are:
 *                    PTHREAD_CREATE_DETACHED - new threads created with attr are in the detached state
 *                    PTHREAD_CREATE_JOINABLE - new threads created with attr are in the joinable state
 * @return 0 is returned upon the successfull execution. Otherwise -1 is returned and errno is set to the error code.
 */
int   pthread_attr_setdetachstate(pthread_attr_t * attr, int detachstate);

/**
 * @brief Sets the guardsize attribute from the attr structure
 * 
 * This function sets the guardsize attribute of the attr structure. This attribute controls the guard area created for the created thread's stack which
 * protects against the stack overflow. This area is created on the end of the stack and whet it is reached, a SIGSEGV is sent to the application. The
 * default value of the attribute if no pthread_attr_setguardsize() calls have been made is PAGESIZE. If it is set to 0, new thread should not have
 * a guard region for the stack.
 * 
 * @param attrs_ptr Pointer to the pthread_attr_t which attribute should be set.
 * @param guardsize the size of the guard area. The actual size will be rounded up to the PAGESIZE, however the call to the pthread_attr_getguardsize() shall
 *                  return the value set by this function without rounding. 0 indicates that the newly created thread will not have a guard region.
 * @return 0 if successfull. If the error is produced, -1 is returned and errno is set to the error.
 */
int   pthread_attr_setguardsize(pthread_attr_t * attr, size_t guardsize);

/**
 * @brief Returns the value of the guardsize attribute.
 * 
 * This function returns the value of the guardsize attribute. This value can be set by pthread_attr_setguardsize(). Please see its documentation for the
 * better explanation of the attribute.
 * 
 * @param attr Pointer to the pthread_attr_t structure holding the arguments.
 * @param guardsize In case of the successful execution, the pointer is set to the guardsize value.
 * @return 0 if the execution was successful. Otherwise, -1 is returned and errno is set to the error code. 
 */
int   pthread_attr_getguardsize(const pthread_attr_t * attr, size_t * guardsize);


int   pthread_attr_getinheritsched(const pthread_attr_t *, int *);
int   pthread_attr_getschedparam(const pthread_attr_t *,
          struct sched_param *);
int   pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
int   pthread_attr_getscope(const pthread_attr_t *, int *);
int   pthread_attr_getstackaddr(const pthread_attr_t *, void **);
int   pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
int   pthread_attr_init(pthread_attr_t *);
int   pthread_attr_setinheritsched(pthread_attr_t *, int);
int   pthread_attr_setschedparam(pthread_attr_t *,
          const struct sched_param *);
int   pthread_attr_setschedpolicy(pthread_attr_t *, int);
int   pthread_attr_setscope(pthread_attr_t *, int);
int   pthread_attr_setstackaddr(pthread_attr_t *, void *);
int   pthread_attr_setstacksize(pthread_attr_t *, size_t);

int   pthread_cancel(pthread_t);
void  pthread_cleanup_push(void (*)(void), void *);
void  pthread_cleanup_pop(int);
int   pthread_cond_broadcast(pthread_cond_t *);
int   pthread_cond_destroy(pthread_cond_t *);
int   pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int   pthread_cond_signal(pthread_cond_t *);
int   pthread_cond_timedwait(pthread_cond_t *, 
          pthread_mutex_t *, const struct timespec *);
int   pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int   pthread_condattr_destroy(pthread_condattr_t *);
int   pthread_condattr_getpshared(const pthread_condattr_t *, int *);
int   pthread_condattr_init(pthread_condattr_t *);
int   pthread_condattr_setpshared(pthread_condattr_t *, int);

/**
 * @brief Create a new thread.
 *
 * This function creates a new thread of execution within the calling process.
 *
 * @param thread Pointer to the thread identifier of the newly created thread.
 * @param attr Pointer to thread attributes (can be NULL for default attributes).
 * @param start_routine Pointer to the function that the new thread will execute.
 * @param arg Argument to pass to the `start_routine` function.
 * 
 * @return 0 on success, or an error code on failure.
 *
 * @note The `thread` argument must be a pointer to a `pthread_t` variable where
 *       the thread identifier will be stored.
 * @note The `attr` argument can be used to specify thread attributes such as
 *       stack size or scheduling policy. Pass `NULL` for default attributes.
 * @note The `start_routine` is the function that the new thread will run. It
 *       should have the following signature: `void* start_routine(void* arg)`.
 * @note The `arg` argument is a pointer to data that will be passed as an
 *       argument to the `start_routine` function.
 * @note The return value indicates success (0) or an error code (non-zero).
 *
 * Example usage:
 * @code
 * pthread_t my_thread;
 * int result = pthread_create(&my_thread, NULL, my_function, NULL);
 * if (result == 0) {
 *     // Thread creation succeeded.
 * } else {
 *     // Thread creation failed, handle the error.
 * }
 * @endcode
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

int   pthread_detach(pthread_t);
int   pthread_equal(pthread_t, pthread_t);
void  pthread_exit(void *);
int   pthread_getconcurrency(void);
int   pthread_getschedparam(pthread_t, int *, struct sched_param *);
void *pthread_getspecific(pthread_key_t);
int   pthread_join(pthread_t, void **);
int   pthread_key_create(pthread_key_t *, void (*)(void *));
int   pthread_key_delete(pthread_key_t);
int   pthread_mutex_destroy(pthread_mutex_t *);
int   pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
int   pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int   pthread_mutex_lock(pthread_mutex_t *);
int   pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);
int   pthread_mutex_trylock(pthread_mutex_t *);
int   pthread_mutex_unlock(pthread_mutex_t *);
int   pthread_mutexattr_destroy(pthread_mutexattr_t *);

int   pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *,
          int *);
int   pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_init(pthread_mutexattr_t *);
int   pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int   pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int   pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int   pthread_mutexattr_settype(pthread_mutexattr_t *, int);

int   pthread_once(pthread_once_t *, void (*)(void));
int   pthread_rwlock_destroy(pthread_rwlock_t *);
int   pthread_rwlock_init(pthread_rwlock_t *,
          const pthread_rwlockattr_t *);
int   pthread_rwlock_rdlock(pthread_rwlock_t *);
int   pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int   pthread_rwlock_trywrlock(pthread_rwlock_t *);
int   pthread_rwlock_unlock(pthread_rwlock_t *);
int   pthread_rwlock_wrlock(pthread_rwlock_t *);
int   pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int   pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *,
          int *);
int   pthread_rwlockattr_init(pthread_rwlockattr_t *);
int   pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
pthread_t
      pthread_self(void);
int   pthread_setcancelstate(int, int *);
int   pthread_setcanceltype(int, int *);
int   pthread_setconcurrency(int);
int   pthread_setschedparam(pthread_t, int ,
          const struct sched_param *);
int   pthread_setspecific(pthread_key_t, const void *);
void  pthread_testcancel(void);


/**
 * @brief Spin lock type for thread synchronization.
 *
 * The `pthread_spinlock_t` type represents a spin lock, which is a simple form of
 * thread synchronization. It provides mutual exclusion and is useful in scenarios
 * where the critical section is expected to be held for a short duration.
 */
typedef int pthread_spinlock_t;

/**
 * @brief Try to acquire a spin lock.
 *
 * This function attempts to acquire the given spin lock. If the lock is currently
 * available, it is acquired and the function returns immediately with a value of 0.
 * If the lock is currently held by another thread, the function returns EBUSY without
 * acquiring the lock.
 *
 * @param lock Pointer to the spin lock variable.
 * @return 0 if the lock was successfully acquired, EBUSY otherwise.
 */
int pthread_spin_trylock(pthread_spinlock_t *lock);

/**
 * @brief Acquire a spin lock.
 *
 * This function acquires the given spin lock. If the lock is currently held by another
 * thread, the calling thread will spin in a busy loop until the lock becomes available.
 *
 * @param lock Pointer to the spin lock variable.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_spin_lock(pthread_spinlock_t *lock);

/**
 * @brief Release a spin lock.
 *
 * This function releases the given spin lock, allowing other threads to acquire it.
 *
 * @param lock Pointer to the spin lock variable.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_spin_unlock(pthread_spinlock_t *lock);

/**
 * @brief Initialize a spinlock.
 *
 * The `pthread_spin_init` function initializes the spinlock pointed to by `lock`.
 *
 * @param lock Pointer to the spinlock to be initialized.
 * @param pshared If `pshared` has a non-zero value, then the spinlock can be shared
 *        between multiple processes; otherwise, the spinlock is only visible to the
 *        threads of the calling process.
 * @return 0 on success, or an error code on failure.
 * @retval EINVAL The value specified by `pshared` is invalid.
 * @retval ENOMEM Insufficient memory exists to initialize the spinlock.
 */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared);

/**
 * @brief Destroy a spinlock.
 *
 * The `pthread_spin_destroy` function destroys the spinlock pointed to by `lock`.
 *
 * @param lock Pointer to the spinlock to be destroyed.
 * @return 0 on success, or an error code on failure.
 * @retval EBUSY The spinlock is locked.
 * @retval EINVAL The value specified by `lock` is invalid.
 */
int pthread_spin_destroy(pthread_spinlock_t *lock);

/**
 * @brief Register fork handlers for thread synchronization.
 *
 * The `pthread_atfork` function registers handler functions to be called when a new
 * process is created with `fork`. This function allows you to specify handlers to
 * perform necessary synchronization tasks before and after the fork operation. The
 * handlers are called in the parent process before fork, in the child process after
 * fork, and in the child process after fork when exec is successful.
 *
 * The `prepare` function is called in the parent process before fork and should perform
 * any necessary locking or cleanup to prepare for a fork operation.
 *
 * The `parent` function is called in the parent process after fork and should reinitialize
 * any locks or resources that were held by the parent before the fork operation.
 *
 * The `child` function is called in the child process after fork when exec is not called.
 * It should release any locks or resources that were held by the parent before the fork
 * operation but should not reinitialize them since the child will typically start with
 * a clean state.
 *
 * @param prepare Pointer to the function to be called in the parent process before fork.
 * @param parent  Pointer to the function to be called in the parent process after fork.
 * @param child   Pointer to the function to be called in the child process after fork.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _PTHREAD_H