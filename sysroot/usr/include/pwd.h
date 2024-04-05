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

#ifndef _PWD_H
#define _PWD_H 1

#define __DECLARE_UID_T
#define __DECLARE_GID_T
#define __DECLARE_SIZE_T
#include "__posix_types.h"

struct passwd {
    char *pw_name; //< User's login name.
    uid_t pw_uid; //< Numerical user ID.
    gid_t pw_gid; //< Numerical group ID.
    char *pw_dir; //< Initial working directory.
    char *pw_shell; //< Program to use as shell.
    char *pw_gecos; //< Real name.
};

#ifdef __STDC_HOSTED__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get user account information.
 *
 * The `getpwnam` function searches the user database for an entry with a matching
 * `name` field.
 *
 * @param name The login name to search for.
 * @return     A pointer to a `passwd` structure containing the user account information,
 *             or `NULL` if no matching entry was found or an error occurred.
 *
 * @note       The `getpwnam` function is a cancellation point.
 */
struct passwd *getpwnam(const char *name);

/**
 * @brief Get user account information.
 *
 * The `getpwuid` function searches the user database for an entry with a matching
 * `uid` field.
 *
 * @param uid The user ID to search for.
 * @return    A pointer to a `passwd` structure containing the user account information,
 *            or `NULL` if no matching entry was found or an error occurred.
 *
 * @note      The `getpwuid` function is a cancellation point.
 */
struct passwd *getpwuid(uid_t uid);

/**
 * @brief Get user account information.
 *
 * The `getpwent` function searches the user database for the next entry with a matching
 * `name` field.
 *
 * @return     A pointer to a `passwd` structure containing the user account information,
 *             or `NULL` if no matching entry was found or an error occurred.
 *
 * @note       The `getpwent` function is a cancellation point.
 */
struct passwd *getpwent(void);

/**
 * @brief Rewind the user account database.
 *
 * The `setpwent` function resets the user account database to allow repeated searches.
 *
 * @return     This function does not return a value.
 *
 * @note       The `setpwent` function is a cancellation point.
 */
void setpwent(void);

/**
 * @brief Close the user account database.
 *
 * The `endpwent` function closes the user account database.
 *
 * @return     This function does not return a value.
 *
 * @note       The `endpwent` function is a cancellation point.
 */
void endpwent(void);

/**
 * @brief Get user account information.
 * 
 * The `getpwnam_r` function searches the user database for an entry with a matching
 * `name` field.
 * 
 * @param name  The login name to search for.
 * @param pwd   A pointer to a `passwd` structure to be filled with the user account information.
 * @param buf   A pointer to a buffer to store the user account information in.
 * @param buflen The size of the buffer.
 * @param result A pointer to a `passwd` structure to be filled with the user account information.
 * @return      If the function succeeds, it returns `0`. Otherwise, it returns an error number.
 * 
 * @note        The `getpwnam_r` function is a cancellation point.
 */
int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result);

/**
 * @brief Get user account information.
 * 
 * The `getpwuid_r` function searches the user database for an entry with a matching
 * `uid` field.
 * 
 * @param uid   The user ID to search for.
 * @param pwd   A pointer to a `passwd` structure to be filled with the user account information.
 * @param buf   A pointer to a buffer to store the user account information in.
 * @param buflen The size of the buffer.
 * @param result A pointer to a `passwd` structure to be filled with the user account information.
 * @return      If the function succeeds, it returns `0`. Otherwise, it returns an error number.
 * 
 * @note        The `getpwuid_r` function is a cancellation point.
 */
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __STDC_HOSTED__ */

#endif /* _PWD_H */