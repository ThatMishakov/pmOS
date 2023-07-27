#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <pmos/ports.h>
#include "string.h"
#include <pmos/ipc.h>

struct fs_task;
struct fs_mountpoint;
struct Path_Node;

/// Flat set of the mountpoints
struct fs_mountpoints_set {
    struct fs_mountpoint **mountpooints;
    size_t mountpoints_count;
    size_t mountpoints_capacity;
    #define MOUNTPOINTS_INITIAL_CAPACITY 4
    #define MOUNTPOINTS_CAPACITY_MULTIPLIER 2
    #define MOUNTPOINTS_SHRINK_THRESHOLD 1/4
};

/**
 * @brief Adds the given mountpoint to the set
 * 
 * @param mountpoint mountpoint to be added
 * @param set Set where it shall be added
 * @return int result of the operation. 0 on success, negative otherwise
 */
int add_mountpoint_to_set(struct fs_mountpoints_set *set, struct fs_mountpoint *mountpoint);

/**
 * @brief Removes mountpoint from the set
 * 
 * @param mountpoint Mountpoint to be removed
 * @param set Set from where it should be removed
 */
void remove_mountpoint_from_set(struct fs_mountpoints_set *set, struct fs_mountpoint *mountpoint);

/**
 * @brief Initializes the given mountpoint set
 * 
 * @param set Set to be initialized
 * @return int result of the operation. 0 on success, negative otherwise
 */
int init_moiuntpoints_set(struct fs_mountpoints_set *set);

/**
 * @brief Frees the buffers within the given mountpoint set
 * 
 * @param set Set whoose buffers are needed to be freed
 */
void mountpoints_set_free_buffers(struct fs_mountpoints_set *set);

/**
 * @brief Gets the last node from the mountpoints set
 * 
 * @param set Set from which the node should be obtained
 * @return struct fs_mountpoint* Last node of the set. NULL if the set is empty or invalid
 */
struct fs_mountpoint *mountpoints_set_back(const struct fs_mountpoints_set *set);

extern struct fs_mountpoints_set global_mountpoints_set; 



struct Filesystem {
    /// ID of the filesystem
    uint64_t id;

    struct File_Request *requests_head, *requests_tail;
    size_t requests_count;

    /// Port used for communication with the filesystem driver
    pmos_port_t command_port;

    /// Flat set of the tasks using this filesystem
    struct fs_task **tasks;
    size_t tasks_count;
    size_t tasks_capacity;
    #define TASKS_INITIAL_CAPACITY 4
    #define TASKS_CAPACITY_MULTIPLIER 2
    #define TASKS_SHRINK_THRESHOLD 1/4

    struct fs_mountpoints_set mountpoints;

    /// Name of the filesystem
    struct String name;
};

/// @brief A hash table of filesystems.
/// @note There is no good reason to use a hash table with linear probing
///       here and with separate chaining in fs_consumer.h other than experimenting
///       with different hash table algorithms
struct filesystem_map {
    /// Filesystems in the map
    struct Filesystem **filesystems;
    /// Number of filesystems in the map
    size_t filesystems_count;
    /// Capacity of the filesystems array
    size_t filesystems_capacity;
    #define FILESYSTEMS_MAP_INITIAL_CAPACITY 4
    #define FILESYSTEMS_MAP_LOAD_FACTOR 3/4
    #define FILESYSTEMS_MAP_CAPACITY_MULTIPLIER 2
};

/// @brief Finds a filesystem in a filesystem map by its ID.
/// @param map The filesystem map to search in.
/// @param fs_id The ID of the filesystem to find.
/// @return The filesystem with the given ID, or NULL if the filesystem is not in the map.
struct Filesystem * find_filesystem_in_map(struct filesystem_map *map, uint64_t fs_id);

/// @brief Adds a filesystem to a filesystem map.
/// @param map The filesystem map to add the filesystem to.
/// @param fs The filesystem to add to the map.
/// @return 0 on success, negative value on failure.
int add_filesystem_to_map(struct filesystem_map *map, struct Filesystem *fs);

/// @brief Removes a filesystem from a filesystem map.
/// @param map The filesystem map to remove the filesystem from.
/// @param fs The filesystem to remove from the map.
void remove_filesystem_from_map(struct filesystem_map *map, struct Filesystem *fs);



/// @brief Creates a new filesystem object.
/// @return The newly created filesystem object.
struct Filesystem * create_filesystem(const char * name, size_t name_length);

/// @brief Gets a filesystem by its ID.
/// @param fs_id The ID of the filesystem.
/// @return The filesystem with the given ID, or NULL if no such filesystem exists.
struct Filesystem * get_filesystem(uint64_t fs_id);

/// @brief Destroys a filesystem object.
/// @param fs The filesystem to destroy.
void destroy_filesystem(struct Filesystem *fs);

/// @brief Adds a task to a filesystem.
/// @param fs The filesystem to add the task to.
/// @param task The task to add to the filesystem.
/// @return 0 on success, negative value on failure.
int add_task_to_filesystem(struct Filesystem *fs, struct fs_task *task);

/// @brief Removes a task from a filesystem.
/// @param fs The filesystem to remove the task from.
/// @param task The task to remove from the filesystem.
void remove_task_from_filesystem(struct Filesystem *fs, struct fs_task *task);

struct fs_task {
    /// ID of the task
    uint64_t id;

    /// Flat set of the filesystems used by this task
    struct Filesystem **filesystems;
    size_t filesystems_count;
    size_t filesystems_capacity;
    #define FILESYSTEMS_INITIAL_CAPACITY 4
};

struct fs_task_map {
    struct fs_task **tasks;
    size_t tasks_count;
    size_t tasks_capacity;
    #define TASKS_INITIAL_CAPACITY 4
    #define TASKS_LOAD_FACTOR 3/4
    #define TASKS_CAPACITY_MULTIPLIER 2
    #define TASKS_SHRINK_THRESHOLD 1/4
};

/// @brief Finds a task in a task map.
/// @param map The task map to search in.
/// @param task_id The ID of the task to search for.
/// @return The task with the given ID, or NULL if no such task exists.
struct fs_task * find_task_in_map(struct fs_task_map *map, uint64_t task_id);

/// @brief Adds a task to the task map.
/// @param map The task map to add the task to.
/// @param task The task to add to the task map.
/// @return 0 on success, negative value on failure.
int add_task_to_map(struct fs_task_map *map, struct fs_task *task);

/// @brief Removes a task from the task map.
/// @param map The task map to remove the task from.
/// @param task The task to remove from the task map.
void remove_task_from_map(struct fs_task_map *map, struct fs_task *task);



/// @brief Creates a new fs_task object.
/// @param id The ID of the task.
struct fs_task * create_fs_task(uint64_t id);

/// @brief Gets a fs_task by its ID.
/// @param task_id The ID of the fs_task.
/// @return The fs_task with the given ID, or NULL if no such fs_task exists.
struct fs_task * get_fs_task(uint64_t task_id);

/// @brief Destroys a fs_task object.
/// @param task The fs_task to destroy.
void destroy_fs_task(struct fs_task *task);

/// @brief Adds a filesystem to a task.
/// @param task The task to add the filesystem to.
/// @param fs The filesystem to add to the task.
/// @return 0 on success, negative value on failure.
int add_filesystem_to_task(struct fs_task *task, struct Filesystem *fs);

/// @brief Removes a filesystem from a task.
/// @param task The task to remove the filesystem from.
/// @param fs The filesystem to remove from the task.
void remove_filesystem_from_task(struct fs_task *task, struct Filesystem *fs);

/// @brief Registeres a task with the filesystem object
/// @param fs The filesystem object to register the task with
/// @param task The task to register
/// @return 0 on success, negative value on failure
int register_task_with_filesystem(struct Filesystem *fs, struct fs_task *task);

/// @brief Unregisteres a task from the filesystem object
/// @param fs The filesystem object to unregister the task from
/// @param task The task to unregister
void unregister_task_from_filesystem(struct Filesystem *fs, struct fs_task *task);

/// @brief Registers a new filesystem with the given task
/// @param msg The message containing the request
/// @param sender The sender of the message
/// @param size The size of the message
/// @return 0 on success, negative value on failure
int register_fs(IPC_Register_FS *msg, uint64_t sender, size_t size);

/// @brief Gets the task pointer from the given filesystem
/// @param fs The filesystem to search in
/// @param task_id The ID of the task to search for
/// @return The task pointer, or NULL if no such task is registered with filesystem
struct fs_task * get_task_from_filesystem(struct Filesystem *fs, uint64_t task_id);


/// @brief A representation of a mountpoint.
///
/// In my vision, a single filesystem represents a single file tree, which 
/// can then be mounted at multiple mountpoints.
struct fs_mountpoint {
    /// Pointer to the filesystem that is mounted
    struct Filesystem *fs;

    /// Pointer to the node representing the mountpoint root
    struct Path_Node *node;

    /// ID of the mountpoint
    uint64_t mountpoint_id;
};

/// @brief Creates a new mountpoint object and registers it with the given filesystem
/// @param fs The filesystem to mount
/// @param node Node representing the mountpoint root
/// @return The newly created mountpoint object. NULL on error.
struct fs_mountpoint *create_mountpoint(struct Filesystem *fs, struct Path_Node *node);

/// @brief Destroys a given mountpoint and frees its memory. If fs != NULL, removes it from the filesystem
/// @param mountpoint Mountpoint to be destroyed
void destroy_mountpoint(struct fs_mountpoint *mountpoint);

#endif // FILESYSTEM_H