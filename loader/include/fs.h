#ifndef FS_H
#define FS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pmos/ipc.h>
#include "task_list.h"

struct fs_entry {
    const char *name;
    size_t name_size;

    int type;
    union {
        struct {
            size_t file_size;
            const char *data;
            struct task_list_node *task;
        } file;

        struct {
            struct fs_entry **entries;
            size_t entry_count;
            size_t entry_capacity;
        } directory;
    };
};

/// @brief Destroys a fs_entry struct.
///
/// This function frees all memory buffers allocated by the fs_entry struct
/// and then frees the struct itself. It does not free child entries or
/// task_list_node structs as it does not own them.
/// @param entry Entry to be destroyed
void destroy_fs_entry(struct fs_entry *entry);

/**
 * @brief Adds a child entry to the given directory.
 * 
 * @param directory Directory to add the child to
 * @param entry Entry to add
 * @return int 0 on success, -1 on failure
 */
int fs_entry_add_child(struct fs_entry *directory, struct fs_entry *entry);


struct open_file_bucket {
    struct open_file_bucket *next;
    struct open_file *file;
};

#define OPEN_FILE_BUCKET_START_SIZE 16
#define OPEN_FILE_LOAD_FACTOR 3/4
#define OPEN_FILE_GROWTH_FACTOR 2
#define OPEN_FILE_SHRINK_FACTOR 1/2

#define TASKS_START_SIZE 8
#define TASKS_GROWTH_FACTOR 2

#define CONSUMER_LOAD_FACTOR 3/4
#define CONSUMER_GROWTH_FACTOR 2
#define CONSUMER_BUCKET_START_SIZE 16
#define CONSUMER_SHRINK_FACTOR 1/2

struct fs_consumer {
    uint64_t id;

    // Flat set of task IDs
    uint64_t *tasks;
    size_t task_count;
    size_t task_capacity;

    // Hash map of open files
    struct open_file_bucket **open_files;
    size_t open_files_size;
    size_t open_files_count;
};

struct fs_consumer_bucket {
    struct fs_consumer_bucket *next;
    struct fs_consumer *consumer;
};

enum fs_data_status {
    FS_DATA_UNINITIALIZED = 0,
    FS_DATA_INITIALIZED,
    FS_DATA_REGISTERING,
    FS_DATA_REGISTERED,
    FS_DATA_MOUNTING,
    FS_DATA_MOUNTED,
};

struct fs_data {
    size_t entries_size;
    size_t entries_capacity;
    // Flat array of pointers to fs_entry
    struct fs_entry **entries;

    size_t consumer_count;
    size_t consumer_table_size;
    struct fs_consumer_bucket **consumers;    

    size_t open_files_count;
    size_t open_files_table_size;
    struct open_file_bucket **fs_open_files;

    uint64_t next_open_file_id;
    enum fs_data_status status;
};

/**
 * @brief Adds an entry to the filesystem.
 * 
 * @param data fs_data struct to add the entry to
 * @param entry entry to add
 * @return int 0 on success, -1 on failure
 */
int fs_data_push_entry(struct fs_data *data, struct fs_entry *entry);

/**
 * @brief Reserves space for entries in the filesystem.
 * 
 * @param data fs_data struct to reserve space in
 * @param count number of entries to reserve space for
 * @return int 0 on success, -1 on failure
 */
int fs_data_reserve_entries(struct fs_data *data, size_t count);

/**
 * @brief Clears the filesystem.
 * 
 * @param data fs_data struct to clear
 */
void fs_data_clear(struct fs_data *data);

/**
 * @brief Get consumer by ID.
 * 
 * @param data fs_data struct to search
 * @param id ID of the consumer to get
 * @return struct fs_consumer* pointer to the consumer. NULL if not found.
 */
struct fs_consumer *get_fs_consumer(struct fs_data *data, uint64_t id);

int init_fs();

/**
 * @brief Adds a consumer to the fs_data struct.
 * 
 * @param data fs_data struct to add the consumer to
 * @param consumer consumer to add
 * @return int 0 on success, -1 on failure
 */
int fs_data_add_consumer(struct fs_data *data, struct fs_consumer *consumer);

/**
 * @brief Removes a consumer from the fs_data struct.
 * 
 * @param data fs_data struct to remove the consumer from
 * @param consumer consumer to remove
 */
void fs_data_remove_consumer(struct fs_data *data, struct fs_consumer *consumer);

/**
 * @brief Adds an open file to the fs_data struct.
 * 
 * @param data fs_data struct to add the open file to
 * @param file open file to add
 * @return int 0 on success, -1 on failure
 */
int fs_data_add_open_file(struct fs_data *data, struct open_file *file);

/**
 * @brief Removes an open file from the fs_data struct.
 * 
 * @param data fs_data struct to remove the open file from
 * @param file open file to remove
 */
void fs_data_remove_open_file(struct fs_data *data, struct open_file *file);

int init_consumer_open_files_map(struct fs_consumer *consumer);

/**
 * @brief Adds a consumer to the filesystem.
 * 
 * @param consumer Pointer to the consumer to add
 * @param task_id ID of the task to add
 * @return int 0 on success, -1 on failure
*/
int add_consumer(struct fs_consumer *consumer, uint64_t task_id);

/**
 * @brief Inserts an open file into the filesystem.
 * 
 * @param consumer Pointer to the consumer to add the file to
 * @param file Pointer to the file to add
 * @return int 0 on success, -1 on failure
 */
int insert_open_file(struct fs_consumer *consumer, struct open_file *file);

/**
 * @brief Removes an open file from the filesystem.
 * 
 * @param consumer Pointer to the consumer to remove the file from
 * @param file Pointer to the file to remove
 * @return int 0 on success, -1 on failure
 */
int remove_open_file(struct fs_consumer *consumer, struct open_file *file);

struct open_file {
    uint64_t file_id;
    uint64_t seek_pos;

    struct fs_entry *file;
    struct fs_consumer *consumer;
};

/**
 * @brief Destroys an open file struct.
 * 
 * This function frees all the buffers inside the open_file struct
 * and then frees the struct itself.
 * 
 * @param file Pointer to the open file to destroy
 */
void destroy_open_file(struct open_file *file);

/**
 * @brief Opens a file for reading.
 * 
 * @param consumer Consumer to open the file for
 * @param path Path to the file
 * @param file Pointer to the file pointer to be set
 * @return int 0 on success, -1 on failure
 */
int open_file(struct fs_consumer *consumer, const char *path, struct open_file **file);

/**
 * @brief Registers an open request for the filesystem.
 * 
 * @param msg Pointer to the open request message
 * @param reply Pointer to the open request reply
 * @return int 0 on success, -1 on failure
*/
int register_open_request(IPC_FS_Open *msg, IPC_FS_Open_Reply *reply);

/**
 * @brief Initializes the filesystem and attempts to register it with the VFS dameon.
 * 
 * @param vfsd_port Port of the VFS daemon
 */
void initialize_filesystem(pmos_port_t vfsd_port);

/**
 * @brief React to the register reply message from the VFS daemon.
 * 
 * @param reply Pointer to the register reply message
 * @param reply_size Size of the register reply message
 * @param sender ID of the sender
 * @return int 0 on success, -1 on failure
 */
int fs_react_register_reply(IPC_Register_FS_Reply *reply, size_t reply_size, uint64_t sender);

/**
 * @brief React to the mount reply message from the VFS daemon.
 * 
 * @param reply Reply message
 * @param reply_size Size of the message
 * @param sender Message sender
 * @return int 0 on success, negative on failure
 */
int fs_react_mount_reply(IPC_Mount_FS_Reply *reply, size_t reply_size, uint64_t sender);

#endif /* FS_H */