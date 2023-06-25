#ifndef FS_H
#define FS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct fs_entry {
    char *name;
    size_t size;
    char *data;
};

struct fs_data {
    size_t size;
    size_t capacity;
    struct fs_entry *entries;
};

void init_fs();



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
    struct open_file *next;
    uint64_t seek_pos;
    uint64_t file_id;

    struct fs_entry *file;
    struct fs_consumer *consumer;
};

struct open_file_list {
    // Hash table of open files
    struct open_file **files;
    size_t capacity;
    size_t count;
};

struct open_file_list open_files = {
    .files = NULL,
    .capacity = 0,
    .count = 0
};

/**
 * @brief Opens a file for reading.
 * 
 * @param consumer Consumer to open the file for
 * @param path Path to the file
 * @param file Pointer to the file pointer to be set
 * @return int 0 on success, -1 on failure
 */
int open_file(struct fs_consumer *consumer, const char *path, struct open_file **file);



#endif /* FS_H */