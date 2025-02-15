/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H 1

#define __DECLARE_SIZE_T
// POSIX doesn't seem to mention NULL here, but it's pulled from
// time.h and everyone expects it
#define __DECLARE_NULL
#define __DECLARE_PTHREAD_COND_T
#include "__posix_types.h"
#include "sys/types.h"

#include <sched.h>
#include <time.h>

#define PTHREAD_CANCEL_ENABLE  (0x01)
#define PTHREAD_CANCEL_DISABLE (0x02)

#define PTHREAD_CANCEL_DEFERRED     (0x01)
#define PTHREAD_CANCEL_ASYNCHRONOUS (0x02)

#define PTHREAD_CANCELLED (-1)

#define PTHREAD_COND_INITIALIZER {NULL, NULL, 0}

#define PTHREAD_CREATE_JOINABLE (0x00)
#define PTHREAD_CREATE_DETACHED (0x01)

#define PTHREAD_INHERIT_SCHED  (0x01)
#define PTHREAD_EXPLICIT_SCHED (0x02)

#define PTHREAD_MUTEX_INITIALIZER                     \
    ((pthread_mutex_t) {.block_count          = 0x00, \
                        .blocking_thread_id   = 0,    \
                        .waiters_list_head    = NULL, \
                        .waiters_list_tail    = NULL, \
                        .recursive_lock_count = 0,    \
                        .type                 = 0})

#define PTHREAD_ONCE_INIT ((pthread_once_t)(0x00))

#define PTHREAD_MUTEX_NORMAL     (0x01)
#define PTHREAD_MUTEX_ERRORCHECK (0x02)
#define PTHREAD_MUTEX_RECURSIVE  (0x03)
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

#define PTHREAD_PRIO_NONE    (0x01)
#define PTHREAD_PRIO_INHERIT (0x02)
#define PTHREAD_PRIO_PROTECT (0x03)

#define PTHREAD_PROCESS_PRIVATE (0x01)
#define PTHREAD_PROCESS_SHARED  (0x02)

#define PTHREAD_RWLOCK_INITIALIZER \
    {                              \
        PTHREAD_COND_INITIALIZER,  \
        PTHREAD_MUTEX_INITIALIZER, \
                                   \
        0,                         \
        0,                         \
        0,                         \
    }

#define PTHREAD_SCOPE_PROCESS (0x01);
#define PTHREAD_SCOPE_SYSTEM  (0x02);

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Uninitializes the pthread_attr_t structure
 *
 * This function should uninitialize provided pthread_attr_t structure. In pmOS this does nothing
 * and should always return success, however you should call this function anyways for portability
 * of your program.
 *
 * @param attrs_ptr structure to be uninitialized.
 * @return 0 is returned upon the successfull execution. Otherwise, -1 is returned and errno is set
 * to the error code.
 */
int pthread_attr_destroy(pthread_attr_t *attrs_ptr);

/**
 * @brief Gets the detachstate attribute
 *
 * _detachstate_ atribute controls whether the thread is created in a detached state.
 *
 * @param attrs_ptr Pointer to the pthread_attr_t holding the attribute.
 * @param detachstate Sets the pointer to the detachstate attrribute value defined in attrs_ptr.
 * Possible values are: PTHREAD_CREATE_DETACHED - new threads created with attr are in the detached
 * state PTHREAD_CREATE_JOINABLE - new threads created with attr are in the joinable state
 * @return 0 is returned upon the successfull execution. Otherwise -1 is returned and errno is set
 * to the error code.
 */
int pthread_attr_getdetachstate(const pthread_attr_t *attrs_ptr, int *detachstate);

/**
 * @brief Sets the detachstate attribute
 *
 * _detachstate_ atribute controls whether the thread is created in a detached state.
 *
 * @param attrs_ptr Pointer to the pthread_attr_t which attribute should be set.
 * @param detachstate The new value of the pointer. Possible parameters are:
 *                    PTHREAD_CREATE_DETACHED - new threads created with attr are in the detached
 * state PTHREAD_CREATE_JOINABLE - new threads created with attr are in the joinable state
 * @return 0 is returned upon the successfull execution. Otherwise -1 is returned and errno is set
 * to the error code.
 */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);

/**
 * @brief Sets the guardsize attribute from the attr structure
 *
 * This function sets the guardsize attribute of the attr structure. This attribute controls the
 * guard area created for the created thread's stack which protects against the stack overflow. This
 * area is created on the end of the stack and whet it is reached, a SIGSEGV is sent to the
 * application. The default value of the attribute if no pthread_attr_setguardsize() calls have been
 * made is PAGESIZE. If it is set to 0, new thread should not have a guard region for the stack.
 *
 * @param attrs_ptr Pointer to the pthread_attr_t which attribute should be set.
 * @param guardsize the size of the guard area. The actual size will be rounded up to the PAGESIZE,
 * however the call to the pthread_attr_getguardsize() shall return the value set by this function
 * without rounding. 0 indicates that the newly created thread will not have a guard region.
 * @return 0 if successfull. If the error is produced, -1 is returned and errno is set to the error.
 */
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);

/**
 * @brief Returns the value of the guardsize attribute.
 *
 * This function returns the value of the guardsize attribute. This value can be set by
 * pthread_attr_setguardsize(). Please see its documentation for the better explanation of the
 * attribute.
 *
 * @param attr Pointer to the pthread_attr_t structure holding the arguments.
 * @param guardsize In case of the successful execution, the pointer is set to the guardsize value.
 * @return 0 if the execution was successful. Otherwise, -1 is returned and errno is set to the
 * error code.
 */
int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize);

int pthread_attr_getinheritsched(const pthread_attr_t *, int *);
int pthread_attr_getschedparam(const pthread_attr_t *, struct sched_param *);
int pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
int pthread_attr_getscope(const pthread_attr_t *, int *);
int pthread_attr_getstackaddr(const pthread_attr_t *, void **);
int pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
int pthread_attr_init(pthread_attr_t *);
int pthread_attr_setinheritsched(pthread_attr_t *, int);
int pthread_attr_setschedparam(pthread_attr_t *, const struct sched_param *);
int pthread_attr_setschedpolicy(pthread_attr_t *, int);
int pthread_attr_setscope(pthread_attr_t *, int);
int pthread_attr_setstackaddr(pthread_attr_t *, void *);
int pthread_attr_setstacksize(pthread_attr_t *, size_t);

int pthread_cancel(pthread_t);
void pthread_cleanup_push(void (*)(void), void *);
void pthread_cleanup_pop(int);
int pthread_cond_broadcast(pthread_cond_t *);
int pthread_cond_destroy(pthread_cond_t *);
int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int pthread_cond_signal(pthread_cond_t *);
int pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);

/**
 * @brief Wait on a condition variable.
 *
 * The `pthread_cond_wait` function atomically releases the mutex pointed to by
 * `mutex` and blocks the calling thread on the condition variable pointed to by
 * `cond`. The thread will remain blocked until another thread signals the
 * condition variable using `pthread_cond_signal` or `pthread_cond_broadcast`,
 * and the current thread is chosen to unblock.
 *
 * Before returning from `pthread_cond_wait`, the mutex is locked again, and
 * this function behaves as if it reacquired the mutex.
 *
 * @param cond  A pointer to the condition variable.
 * @param mutex A pointer to the mutex to be released and reacquired.
 *
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

int pthread_condattr_destroy(pthread_condattr_t *);
int pthread_condattr_getpshared(const pthread_condattr_t *, int *);
int pthread_condattr_init(pthread_condattr_t *);
int pthread_condattr_setpshared(pthread_condattr_t *, int);

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
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *),
                   void *arg);

/**
 * @brief Detach a thread.
 *
 * The `pthread_detach` function is used to mark a thread as detached. A detached
 * thread's resources will be automatically released by the system when it exits.
 * Detached threads cannot be joined with `pthread_join`.
 *
 * @param thread The thread to be detached.
 * @return 0 on success, or an error code on failure.
 *
 * @note Once a thread is detached, it cannot be joined with `pthread_join`.
 *       Attempting to join a detached thread will result in an error.
 */
int pthread_detach(pthread_t thread);

int pthread_equal(pthread_t, pthread_t);
void pthread_exit(void *);
int pthread_getconcurrency(void);
int pthread_getschedparam(pthread_t, int *, struct sched_param *);
int pthread_join(pthread_t, void **);

/**
 * @brief Create a thread-specific data key.
 *
 * This function creates a thread-specific data key that can be used by multiple
 * threads to store and retrieve thread-specific data. The key is associated with
 * a destructor function that is called when a thread exits and the key's data
 * is non-NULL.
 *
 * @param key Pointer to a key variable where the created key will be stored.
 * @param destructor A destructor function to be called when a thread exits, or
 *                   NULL if no destructor is needed.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));

/**
 * @brief Delete a thread-specific data key.
 *
 * This function deletes a thread-specific data key that was previously created
 * with `pthread_key_create`. It does not affect the data associated with the key
 * in individual threads; it only prevents new keys from being created with the
 * specified key.
 *
 * @param key The key to be deleted.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_key_delete(pthread_key_t key);

/**
 * @brief Get the thread-specific data associated with a key.
 *
 * This function retrieves the thread-specific data associated with a key for the
 * calling thread.
 *
 * @param key The key to retrieve data from.
 * @return A pointer to the thread-specific data associated with the key, or NULL
 * if the key is not associated with a value for the calling thread.
 */
void *pthread_getspecific(pthread_key_t key);

/**
 * @brief Set the thread-specific data associated with a key.
 *
 * This function sets the thread-specific data associated with a key for the
 * calling thread.
 *
 * @param key The key to associate the data with.
 * @param value A pointer to the data to be associated with the key.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_setspecific(pthread_key_t key, const void *value);

/**
 * @brief Destroys a mutex attributes object.
 *
 * The pthread_mutexattr_destroy() function deallocates any resources used by
 * the mutex attributes object referred to by attr. After this function is
 * called, the attributes object is no longer valid, and any further use of it
 * results in undefined behavior.
 *
 * @param attr A pointer to the mutex attributes object to be destroyed.
 *
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

int pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);
int pthread_mutex_trylock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_mutexattr_destroy(pthread_mutexattr_t *);

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);

/**
 * @brief Initializes a mutex attributes object with default values.
 *
 * The pthread_mutexattr_init() function initializes the mutex attributes
 * object referred to by attr with default values for all of the individual
 * attributes used by a mutex type. After initialization, the attributes object
 * can be used to specify various attributes when creating a mutex.
 *
 * @param attr A pointer to the mutex attributes object to be initialized.
 *
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);

/**
 * @brief Get the mutex type attribute.
 *
 * The pthread_mutexattr_gettype() function retrieves the mutex type attribute
 * from the attributes object referred to by attr and stores it in the location
 * pointed to by type.
 *
 * @param attr A pointer to the mutex attributes object.
 * @param type A pointer to an integer where the mutex type will be stored.
 *
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);

/**
 * @brief Sets the mutex type attribute in a mutex attributes object.
 *
 * The pthread_mutexattr_settype() function sets the mutex type attribute in
 * the attributes object referred to by attr. The type argument must be one of
 * the following constants:
 *
 * - PTHREAD_MUTEX_NORMAL: This type of mutex does not check for deadlock.
 * - PTHREAD_MUTEX_ERRORCHECK: This type of mutex provides error checking.
 * - PTHREAD_MUTEX_RECURSIVE: This type of mutex allows recursive locking.
 * - PTHREAD_MUTEX_DEFAULT: The default mutex type.
 *
 * @param attr A pointer to the mutex attributes object to be modified.
 * @param type The desired mutex type.
 *
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

/**
 * @brief Destroy a mutex object.
 *
 * This function destroys the mutex object referenced by the `mutex` argument.
 * The mutex must be unlocked before it is destroyed. Attempting to destroy a
 * locked mutex results in undefined behavior.
 *
 * After a mutex has been destroyed, it can be safely reinitialized using
 * `pthread_mutex_init`.
 *
 * @param mutex A pointer to the mutex object to be destroyed.
 * @return 0 on success, or an error code on failure.
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex);

/**
 * @brief Initializes a once-only control variable and executes a specified
 *        initialization routine once and only once across all threads.
 *
 * The pthread_once() function provides a simple and efficient mechanism for
 * ensuring that a specific initialization routine is executed only once,
 * regardless of how many threads attempt to invoke it.
 *
 * @param once_control A pointer to the once-only control variable of type
 *                     pthread_once_t.
 * @param init_routine The initialization routine to be executed once.
 *
 * @return 0 on success, error otherwise.
 */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

/**
 * @brief Initialize a read-write lock.
 *
 * This function initializes a read-write lock with the attributes specified in
 * `attr`. If `attr` is NULL, default attributes are used.
 *
 * @param rwlock Pointer to the read-write lock to be initialized.
 * @param attr Pointer to the attributes object, or NULL for default attributes.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);

/**
 * @brief Destroy a read-write lock.
 *
 * This function destroys a read-write lock previously initialized with
 * `pthread_rwlock_init`. Any threads currently blocked on the lock will be
 * awakened with an error status.
 *
 * @param rwlock Pointer to the read-write lock to be destroyed.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);

/**
 * @brief Acquire a read lock.
 *
 * This function acquires a read lock on the specified read-write lock `rwlock`.
 * Multiple threads can hold read locks simultaneously, but if any thread holds
 * a write lock or requests one, read lock requests will be blocked until the
 * write lock is released.
 *
 * @param rwlock Pointer to the read-write lock to acquire a read lock on.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);

/**
 * @brief Acquire a write lock.
 *
 * This function acquires a write lock on the specified read-write lock `rwlock`.
 * Only one thread can hold a write lock at a time, and if any thread holds a
 * read lock, write lock requests will be blocked until all read locks are
 * released.
 *
 * @param rwlock Pointer to the read-write lock to acquire a write lock on.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);

/**
 * @brief Try to acquire a read lock on a read-write lock.
 *
 * This function attempts to acquire a read lock on the specified read-write lock.
 * If the read lock is already held by another thread in write mode, this function
 * will return immediately with a failure status.
 *
 * @param rwlock A pointer to the read-write lock to be locked.
 * @return 0 if the read lock was successfully acquired, EBUSY if the lock is
 *         held in write mode by another thread, or a positive error code on failure.
 */
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);

/**
 * @brief Attempt to acquire a write lock.
 *
 * This function attempts to acquire a write lock on the specified read-write lock
 * `rwlock`. If the lock is already held by another thread in either read or
 * write mode, the function returns immediately with an error status.
 *
 * @param rwlock Pointer to the read-write lock to attempt to acquire a write lock on.
 * @return 0 if the write lock was successfully acquired, or a positive error code on failure.
 */
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);

/**
 * @brief Release a read or write lock.
 *
 * This function releases a read or write lock held on the specified read-write
 * lock `rwlock`. If multiple threads are waiting for the lock, one of them will
 * be granted the lock.
 *
 * @param rwlock Pointer to the read-write lock to release.
 * @return 0 on success, or a positive error code on failure.
 */
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

/**
 * @brief Try to acquire a read lock on a read-write lock with a timeout.
 *
 * This function attempts to acquire a read lock on the specified read-write lock.
 * If the read lock is already held by another thread in write mode, this function
 * will wait for the specified timeout duration to acquire the lock. If the timeout
 * is reached before the lock is acquired, it will return with a failure status.
 *
 * @param rwlock A pointer to the read-write lock to be locked.
 * @param abstime A pointer to the absolute timeout time.
 * @return 0 if the read lock was successfully acquired, ETIMEDOUT if the timeout
 *         was reached before acquiring the lock, EBUSY if the lock is held in write
 *         mode by another thread, or a positive error code on failure.
 */
int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock, const struct timespec *abstime);

/**
 * @brief Try to acquire a write lock on a read-write lock with a timeout.
 *
 * This function attempts to acquire a write lock on the specified read-write lock.
 * If the write lock is already held by another thread (read or write), this function
 * will wait for the specified timeout duration to acquire the lock. If the timeout
 * is reached before the lock is acquired, it will return with a failure status.
 *
 * @param rwlock A pointer to the read-write lock to be locked.
 * @param abstime A pointer to the absolute timeout time.
 * @return 0 if the write lock was successfully acquired, ETIMEDOUT if the timeout
 *         was reached before acquiring the lock, or a positive error code on failure.
 */
int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock, const struct timespec *abstime);

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *, int *);
int pthread_rwlockattr_init(pthread_rwlockattr_t *);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
pthread_t pthread_self(void);
int pthread_setcancelstate(int, int *);
int pthread_setcanceltype(int, int *);
int pthread_setconcurrency(int);
int pthread_setschedparam(pthread_t, int, const struct sched_param *);
void pthread_testcancel(void);

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

/**
 * @brief Try to acquire a mutex within a specified time limit.
 *
 * This function tries to acquire the specified mutex. If the mutex is already
 * locked by another thread, it will block until the mutex becomes available
 * or the specified timeout expires.
 *
 * @param mutex A pointer to the mutex to be locked.
 * @param abstime A pointer to a structure specifying the absolute time limit.
 * @return 0 if the mutex was successfully acquired within the timeout,
 *         EBUSY if the mutex is already locked, or a positive error code
 *         on failure.
 */
int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime);

int pthread_setname_np(pthread_t *thread, const char *name);
int pthread_getname_np(pthread_t *thread, const char *name, size_t len);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _PTHREAD_H