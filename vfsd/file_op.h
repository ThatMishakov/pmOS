#ifndef FILE_OP_H
#define FILE_OP_H
#include "string.h"
#include <stdint.h>
#include "fs_consumer.h"
#include <pmos/ipc.h>

struct Path_Node;
struct fs_consumer;
struct File_Request {
    /// Implicit linked list
    struct File_Request *path_node_next, *path_node_prev;
    struct File_Request *consumer_req_next, *consumer_req_prev;

    /// Parent filesystem
    struct fs_consumer *consumer;

    /// Port to send the reply to
    uint64_t reply_port;

    /// Full absolute path to the file processed
    struct String path;

    /// Index of the first character within path of the file name currently being processed
    size_t active_file_index;

    /// Pointer to the node currently being parsed
    struct Path_Node *active_node;

    /// ID of the request made by the client
    uint64_t request_id;

    /// Type of request made by the client
    int request_type;
};

/**
 * @brief Processes an open request from a client
 * 
 * @param request Request made by the client 
 * @param sender Task id that made the request
 * @param reply Reply to be sent to the client
 * @return int result of the operation. 0 if successful, -1 otherwise
 */
int open_file(const struct IPC_Open *request, uint64_t sender, uint64_t request_length);

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
 * @brief Binds the request to the root of the filesystem
 * 
 * @param request Request to bind
 * @return int 0 on success, negative value otherwise
 */
int bind_request_to_root(struct File_Request *request);

/**
 * @brief Prepares the filename of the request
 *
 * This function takes the given file path and prepares it for the given request. If the file_path is zero, it uses
 * the consumer's path. If the file_path is an absolute path, it is copied as-is. If the file_path is a relative path,
 * it is appended to the consumer's path. The result is saved in the request's path.
 *
 * @param request The request where the processed filename is saved.
 * @param consumer The consumer to prepare the filename for.
 * @param file_path The path to the file.
 * @param path_length The length of the file path.
 * @return 0 on success, -EINVAL if the request or consumer is NULL, -ENOMEM if memory allocation fails,
 *         or -ENOSPC if the resulting path exceeds the capacity of the request's path.
 *
 * @note The request must be already initialized, and the string should already be initialized.
 * @note If file_path is an empty string, the consumer's path will be used.
 * @note If file_path is an absolute path, it will be copied as-is.
 * @note If file_path is a relative path, it will be appended to the consumer's path.
 * @note The existing contents of request->path will be overridden, leading to potential memory leaks if not properly managed.
 */
int prepare_filename(struct File_Request * request, const struct fs_consumer *consumer, const char * file_path, size_t path_length);

#endif // FILE_OP_H