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

#ifndef FILE_OP_H
#define FILE_OP_H
#include "fs_consumer.h"
#include "string.h"

#include <pmos/ipc.h>
#include <stdbool.h>
#include <stdint.h>

struct Path_Node;
struct fs_consumer;
struct Filesystem;

enum Request_Type {
    REQUEST_TYPE_UNKNOWN = 0,
    REQUEST_TYPE_OPEN_FILE,
    REQUEST_TYPE_OPEN_FILE_RESOLVED,
    REQUEST_TYPE_MOUNT,
    REQUEST_TYPE_RESOLVE_PATH,
    REQUEST_TYPE_DUP,
};

struct File_Request {
    /// Implicit linked list
    struct File_Request *path_node_next, *path_node_prev;
    struct File_Request *consumer_req_next, *consumer_req_prev;
    struct File_Request *fs_req_next, *fs_req_prev;

    /// Parent filesystem
    struct fs_consumer *consumer;
    struct Filesystem *filesystem;

    /// Port to send the reply to
    uint64_t reply_port;

    /// ID of the file in some requests
    uint64_t file_id;

    /// Full absolute path to the file processed
    struct String path;

    /// Index of the first character (including the '/') of the next path node (e.g.
    /// /home/user/file.txt -> 5)
    size_t next_path_node_index;

    /// Pointer to the node currently being parsed
    struct Path_Node *active_node;

    /// ID of the request made by the client
    uint64_t request_id;

    /// Type of request made by the client
    enum Request_Type request_type;
};

struct File_Request_Node {
    struct File_Request_Node *next;
    struct File_Request *request;
};

struct File_Request_Map {
    struct File_Request_Node **hash_vector;
    size_t vector_size;
    size_t nodes_count;
};
#define FILE_REQUEST_HASH_LOAD_FACTOR      3 / 4
#define FILE_REQUEST_HASH_INITIAL_SIZE     4
#define FILE_REQUEST_HASH_SHRINK_THRESHOLD 1 / 4

/**
 * @brief Processes an open request from a client
 *
 * @param request Request made by the client
 * @param sender Task id that made the request
 * @param request_length The length of the request message,
 * @return int result of the operation. 0 if successful, -1 otherwise
 */
int open_file(const struct IPC_Open *request, uint64_t sender, uint64_t request_length);

/**
 * @brief Creates a new RESOLVE_PATH request
 *
 * This function creates a new request to resolve the ID of the file. It allocates the memory as
 * necessary and initializes the request. The request is registered with the child node and in the
 * global request map.
 *
 * @param child_node Node to resolve the path for
 * @return struct File_Request* Pointer to the request. NULL if an error occurred.
 */
struct File_Request *create_resolve_path_request(struct Path_Node *child_node);

/**
 * @brief Processes a mount request
 *
 * This function is similar to open_file. It attempts to prepare and register mount request and then
 * replies to it. On error, it sends the reply to the reply port indicated in the request message,
 * if possible.
 *
 * @param request Request to mount a filesystem made by a driver
 * @param sender Task id that sent the request
 * @param request_length Length of the request message send by the task
 * @return int resilt of the operation. 0 if successfull, negative otherwise.
 */
int mount_filesystem(const struct IPC_Mount_FS *request, uint64_t sender, uint64_t request_length);

/**
 * @brief Initializes an open request
 *
 * @param request Request to initialize
 * @return int result of the operation. 0 if successful, -1 otherwise
 */
int init_open_request(struct File_Request *request);

/**
 * @brief Frees the memory buffers of an open request
 *
 * @param request Request which's buffers to free
 */
void free_buffers_file_request(struct File_Request *request);

/**
 * @brief Register the request with the consumer
 *
 * @param request Request to register
 * @param consumer Consumer to register the request with
 * @return int result of the operation. 0 if successful, -1 otherwise
 */
int register_request_with_consumer(struct File_Request *request, struct fs_consumer *consumer);

/**
 * @brief Register the request with the filesystem
 *
 * This function is very similar to the register_request_with_consumer, except that it registers new
 * request (e.g. mount) with the given filesystem.
 *
 * @param request Request to register
 * @param filesystem Filesystem to register request with
 * @return in retult of the operation. 0 if successful, -1 otherwise
 */
int register_request_with_filesystem(struct File_Request *request, struct Filesystem *filesystem);

/**
 * @brief Unregisters the request from the parent consumer or filesystem
 *
 * This function unregisters the request from the parent consumer or filesystem. If the request is
 * not registered with consumer or filesystem, this function does nothing. The type of the parent is
 * determined by the request_type field.
 *
 * @param request Request to unregister
 */
void unregister_request_from_parent(struct File_Request *request);

/**
 * @brief Removes a request from the Path_Node
 *
 * This function removes a given request from the Path_Node it is registered with. If the request is
 * not registered with any Path_Node, this function does nothing.
 * @param request Request to remove
 */
void remove_request_from_path_node(struct File_Request *request);

/**
 * @brief Adds the request to the given path node
 *
 * This function adds the request that is not bound to anything to the given path node. Caller must
 * make sure it is not already bound to another node
 *
 * @param request Request to add to the node
 * @param node Node where the request should be added
 * @return int 0 on success, negative on error
 */
int add_request_to_path_node(struct File_Request *request, struct Path_Node *node);

/**
 * @brief Binds the request to the root of the filesystem
 *
 * @param request Request to bind
 * @return int 0 on success, negative value otherwise
 */
int bind_request_to_root(struct File_Request *request);

/**
 * @brief Prepares the filename of the request
 *
 * This function takes the given file path and prepares it for the given request. If the file_path
 * is zero, it uses the consumer's path. If the file_path is an absolute path, it is copied as-is.
 * If the file_path is a relative path, it is appended to the consumer's path. The result is saved
 * in the request's path.
 *
 * @param request The request where the processed filename is saved.
 * @param path Path of the filesystem consumer
 * @param path_length Length of the path of the filesystem consumer
 * @param file_path The path to the file.
 * @param path_length The length of the file path.
 * @return 0 on success, -EINVAL if the request or consumer is NULL, -ENOMEM if memory allocation
 * fails, or -ENOSPC if the resulting path exceeds the capacity of the request's path.
 *
 * @note The request must be already initialized, and the string should already be initialized.
 * @note If file_path is an empty string, the consumer's path will be used.
 * @note If file_path is an absolute path, it will be copied as-is.
 * @note If file_path is a relative path, it will be appended to the consumer's path.
 * @note The existing contents of request->path will be overridden, leading to potential memory
 * leaks if not properly managed.
 */
int prepare_filename(struct File_Request *request, const char *path, size_t path_length,
                     const char *filename, size_t filename_length);

/**
 * @brief Processes a request
 *
 * This function takes an ownership of a prepared file request and then processes it as needed. Even
 * if the error is encountered, the memory occupied by the request is freed by this function. If the
 * request is valid, this function always attempts to reply to it and only returns error if it was
 * not possible.
 *
 * @param request Request to be processed
 * @return int Result of the operation. 0 on success, negative if the request is invalid or if the
 * reply could not be sent.
 */
int process_request(struct File_Request *request);

/**
 * @brief Processes requests of the node
 *
 * This function takes a given Path_Node and processes all its pending requests. This function shall
 * be called when the status of the node has changed, e.g. it was resolved or it was found that the
 * file does not exist.
 *
 * @param node Node to process requests for
 * @return int Result of the operation. 0 on success, negative if the node is not valid (NULL) or
 * some serious error was encountered.
 */
int process_requests_of_node(struct Path_Node *node);

/**
 * @brief Fail and destroy the request
 *
 * This function responds to the request with a given message and then destroys it, freeing the
 * associated memory and the request itself. It also removes the requests from all the lists it is
 * currently in.
 *
 * @param request Request to fail
 * @param error_code errno-like error code code to send to the client
 */
void fail_and_destroy_request(struct File_Request *request, int error_code);

/**
 * @brief Checks if the request should be in the global requests map
 *
 * This function checks if the request should be in the global requests map, by checking its type.
 *
 * @param request Request to check
 * @return true if the request should be in the global requests map, false otherwise
 */
bool is_request_in_global_map(struct File_Request *request);

/**
 * @brief Removes the request from global map
 *
 * This function checks if the request is in the global map and if so, removes it from there.
 *
 * @param request Valid request to remove
 */
void remove_request_from_global_map(struct File_Request *request);

/**
 * @brief Adds the request to the requests map
 *
 * @param map Map to add the request to
 * @param request Request to add
 * @return int 0 on success, negative value otherwise
 */
int add_request_to_map(struct File_Request_Map *map, struct File_Request *request);

/**
 * @brief Gets the request from the map by its ID
 *
 * @param map Map to get the request from
 * @param id ID of the request to get
 * @return struct File_Request* Pointer to the request, NULL if not found
 */
struct File_Request *get_request_from_map(struct File_Request_Map *map, uint64_t id);

/**
 * @brief Removes the request from the requests map
 *
 * @param map Map to remove the request from
 * @param request Request to remove
 */
void remove_request_from_map(struct File_Request_Map *map, struct File_Request *request);

/**
 * @brief Gets the request from the global requests map by its ID
 *
 * @param id ID of the request to get
 * @return struct File_Request* Pointer to the request, NULL if not found
 */
struct File_Request *get_request_from_global_map(uint64_t id);

/**
 * @brief Reacts to the IPC_FS_Resolve_Path_Reply message
 *
 * This function reacts to the IPC_FS_Resolve_Path_Reply message (which would normally be sent by
 * path_node_resolve_child) and updates the internal state in accordance with it. It does not take
 * the ownership of the message.
 *
 * @param message Message (reply) to react to
 * @param sender Sender of the message
 * @param message_length Length of the message
 * @return int 0 on success, negative value otherwise
 * @see path_node_resolve_child
 */
int react_resolve_path_reply(struct IPC_FS_Resolve_Path_Reply *message, size_t sender,
                             uint64_t message_length);

/**
 * @brief Reacts to the IPC_FS_Open_Reply message
 *
 * This function reacts to the IPC_FS_Open_Replay message sent by the filesystem server and updates
 * the internal state in accordance with it. It does not take the ownership of the message.
 *
 * @param message Message (reply) to react to
 * @param sender Sender of the message
 * @param message_length Length of the message
 * @return int 0 on success, negative value otherwise
 */
int react_ipc_fs_open_reply(struct IPC_FS_Open_Reply *message, size_t sender,
                            uint64_t message_length);

/**
 * @brief Reacts to the IPC_Dup message
 *
 * This function reacts to the IPC_Dup message sent by the filesystem consumer and attempts to
 * duplicate the file descriptor for the given filesystem consumer. It does not take the ownership
 * of the message.
 *
 * @param message Message to react to
 * @param sender Sender of the message
 * @param message_length Length of the message
 * @return int 0 on success, negative value otherwise
 */
int react_ipc_dup(struct IPC_Dup *message, size_t sender, uint64_t message_length);

/**
 * @brief Reacts to the IPC_FS_Dup_Reply message
 *
 * This function reacts to the IPC_FS_Dup_Reply message sent by the filesystem server and updates
 * the internal state in accordance with it. It does not take the ownership of the message. The
 * functionality is very similar to the react_ipc_fs_open_reply.
 *
 * @param message Message (reply) to react to
 * @param sender Sender of the message
 * @param message_length Length of the message
 * @return int 0 on success, negative value otherwise
 */
int react_ipc_fs_dup_reply(struct IPC_FS_Dup_Reply *message, size_t sender,
                           uint64_t message_length);

/**
 * @brief Reacts to the IPC_Close message
 *
 * This function reacts to the IPC_Close message sent by the filesystem consumer and attempts to
 * close the file descriptor. The reply is not sent to avoid deadlock and is assumed to be
 * successful. It does not take the ownership of the message.
 *
 * @param message Message to react to
 * @param sender Sender of the message
 * @param message_length Length of the message
 * @return int 0 on success, negative value otherwise
 */
int react_ipc_close(struct IPC_Close *message, size_t sender, uint64_t message_length);

#endif // FILE_OP_H