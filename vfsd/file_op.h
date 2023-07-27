#ifndef FILE_OP_H
#define FILE_OP_H
#include "string.h"
#include <stdint.h>
#include "fs_consumer.h"
#include <pmos/ipc.h>

struct Path_Node;
struct fs_consumer;
struct Filesystem;

enum Request_Type {
    REQUEST_TYPE_UNKNOWN = 0,
    REQUEST_TYPE_OPEN_FILE,
    REQUEST_TYPE_MOUNT,
};

struct File_Request {
    /// Implicit linked list
    struct File_Request *path_node_next, *path_node_prev;
    struct File_Request *consumer_req_next, *consumer_req_prev;

    /// Parent filesystem
    union {
        struct fs_consumer *consumer;
        struct Filesystem *filesystem;
    };

    /// Port to send the reply to
    uint64_t reply_port;

    /// ID of the file in some requests
    uint64_t file_id;

    /// Full absolute path to the file processed
    struct String path;

    /// Index of the first character within path of the file name currently being processed
    size_t active_file_index;

    /// Pointer to the node currently being parsed
    struct Path_Node *active_node;

    /// ID of the request made by the client
    uint64_t request_id;

    /// Type of request made by the client
    enum Request_Type request_type;
};

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
 * @brief Processes a mount request
 * 
 * This function is similar to open_file. It attempts to prepare and register mount request and then replies to it.
 * On error, it sends the reply to the reply port indicated in the request message, if possible.
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
 * This function is very similar to the register_request_with_consumer, except that it registers new request (e.g. mount)
 * with the given filesystem.
 * 
 * @param request Request to register
 * @param filesystem Filesystem to register request with
 * @return in retult of the operation. 0 if successful, -1 otherwise
 */
int register_request_with_filesystem(struct File_Request *request, struct Filesystem *filesystem);

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
 * @param path Path of the filesystem consumer
 * @param path_length Length of the path of the filesystem consumer
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
int prepare_filename(struct File_Request * request, const char * path, size_t path_length, const char * filename, size_t filename_length);

/**
 * @brief Processes a request
 * 
 * This function takes an ownership of a prepared file request and then processes it as needed. Even if the error is encountered, the memory occupied
 * by the request is freed by this function. If the request is valid, this function always attempts to reply to it and only returns error if it was
 * not possible.
 * 
 * @param request Request to be processed 
 * @return int Result of the operation. 0 on success, negative if the request is invalid or if the reply could not be sent.
 */
int process_request(struct File_Request * request);

/**
 * @brief Processes requests of the node
 * 
 * This function takes a given Path_Node and processes all its pending requests. This function shall be called when the status of
 * the node has changed, e.g. it was resolved or it was found that the file does not exist.
 * 
 * @param node Node to process requests for
 * @return int Result of the operation. 0 on success, negative if the node is not valid (NULL) or some serious error was encountered.
 */
int process_requests_of_node(struct Path_Node *node);

#endif // FILE_OP_H