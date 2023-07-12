#ifndef FILE_OP_H
#define FILE_OP_H
#include "string.h"
#include <stdint.h>
#include "fs_consumer.h"
#include <pmos/ipc.h>

struct Path_Node;
struct FS_Consumer;
struct Open_Request {
    /// Implicit linked list
    struct Open_Request *path_node_next, *path_node_prev;
    struct Open_Request *consumer_node_next, *consumer_node_prev;

    /// Parent filesystem
    struct FS_Consumer *consumer;

    /// Port to send the reply to
    uint64_t reply_port;

    /// Full absolute path to the file processed
    struct String path;

    /// Pointer to the first character of the file name currently being processed
    const char *active_name;

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
int init_open_request(struct Open_Request *request);

/**
 * @brief Frees the memory buffers of an open request
 * 
 * @param request Request which's buffers to free
 */
void free_buffers_open_request(struct Open_Request *request);

/**
 * @brief Register the request with the consumer
 * 
 * @param request Request to register
 * @param consumer Consumer to register the request with
 * @return int result of the operation. 0 if successful, -1 otherwise
 */
int register_request_with_consumer(struct Open_Request *request, struct fs_consumer *consumer);

#endif // FILE_OP_H