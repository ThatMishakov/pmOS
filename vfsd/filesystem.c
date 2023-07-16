#include "filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pmos/ipc.h>
#include <pmos/system.h>

struct filesystem_map global_filesystem_map = {.filesystems = 0, .filesystems_count = 0, .filesystems_capacity = 0};
struct fs_task_map global_task_map = {.tasks = 0, .tasks_count = 0, .tasks_capacity = 0};

uint64_t get_next_filesystem_id()
{
    static uint64_t next_filesystem_id = 1;
    return __atomic_fetch_add(&next_filesystem_id, 1, __ATOMIC_SEQ_CST);
}

struct Filesystem * create_filesystem(const char * name, size_t name_length)
{
    struct Filesystem * filesystem = (struct Filesystem *)calloc(sizeof(struct Filesystem), 1);
    if (filesystem == NULL)
        return NULL;
    
    if (init_null_string(&filesystem->name) != 0) {
        free(filesystem);
        return NULL;
    }

    if (append_string(&filesystem->name, name, name_length) != 0) {
        destroy_string(&filesystem->name);
        free(filesystem);
        return NULL;
    }

    filesystem->id = get_next_filesystem_id();

    int result = add_filesystem_to_map(&global_filesystem_map, filesystem);
    if (result != 0) {
        destroy_string(&filesystem->name);
        free(filesystem);
        return NULL;
    }

    return filesystem;
}

void destroy_filesystem(struct Filesystem * filesystem)
{
    if (filesystem == NULL)
        return;

    for (size_t i = 0; i < filesystem->tasks_count; ++i)
        remove_filesystem_from_task(filesystem->tasks[i], filesystem);
    free(filesystem->tasks);

    remove_filesystem_from_map(&global_filesystem_map, filesystem);
    destroy_string(&filesystem->name);
    free(filesystem);
}

int add_task_to_filesystem(struct Filesystem *fs, struct fs_task *task)
{
    if (fs == NULL || task == NULL)
        return -EINVAL;

    if (fs->tasks_count == fs->tasks_capacity) {
        size_t new_capacity = (fs->tasks_capacity == 0) ? TASKS_INITIAL_CAPACITY : fs->tasks_capacity * 2;
        struct fs_task **new_tasks = (struct fs_task **)realloc(fs->tasks, new_capacity * sizeof(struct fs_task *));
        if (new_tasks == NULL)
            return -errno;
        
        fs->tasks = new_tasks;
        fs->tasks_capacity = new_capacity;
    }

    // Binary search for the right place to insert the task
    size_t left = 0;
    size_t right = fs->tasks_count;
    while (left < right) {
        size_t middle = (left + right) / 2;
        if (fs->tasks[middle]->id < task->id)
            left = middle + 1;
        else
            right = middle;
    }

    // Check if the task with the same id already exists
    if (left < fs->tasks_count && fs->tasks[left]->id == task->id)
        return -EEXIST;

    // Insert the task
    memmove(fs->tasks + left + 1, fs->tasks + left, (fs->tasks_count - left) * sizeof(struct fs_task *));
    fs->tasks[left] = task;
    fs->tasks_count++;

    return 0;
}

void remove_task_from_filesystem(struct Filesystem *fs, struct fs_task *task)
{
    if (fs == NULL || task == NULL)
        return;

    // Binary search for the task
    size_t left = 0;
    size_t right = fs->tasks_count;
    while (left < right) {
        size_t middle = (left + right) / 2;
        if (fs->tasks[middle]->id < task->id)
            left = middle + 1;
        else
            right = middle;
    }

    // Check if the task with the same id exists
    if (left >= fs->tasks_count || fs->tasks[left]->id != task->id)
        return;

    // Remove the task
    memmove(fs->tasks + left, fs->tasks + left + 1, (fs->tasks_count - left - 1) * sizeof(struct fs_task *));
    fs->tasks_count--;

    // Shrink the array if it's too big
    if (fs->tasks_count * 4 < fs->tasks_capacity && fs->tasks_capacity > TASKS_INITIAL_CAPACITY) {
        size_t new_capacity = fs->tasks_capacity / 2;
        struct fs_task **new_tasks = (struct fs_task **)realloc(fs->tasks, new_capacity * sizeof(struct fs_task *));
        if (new_tasks != NULL) {
            fs->tasks = new_tasks;
            fs->tasks_capacity = new_capacity;
        }
    }
}

struct fs_task * create_fs_task(uint64_t task_id)
{
    struct fs_task * task = (struct fs_task *)calloc(sizeof(struct fs_task), 1);
    if (task == NULL)
        return NULL;

    task->id = task_id;

    int result = add_task_to_map(&global_task_map, task);
    if (result != 0) {
        free(task);
        return NULL;
    }

    return task;
}

void destroy_fs_task(struct fs_task * task)
{
    if (task == NULL)
        return;

    for (size_t i = 0; i < task->filesystems_count; ++i)
        remove_task_from_filesystem(task->filesystems[i], task);
    free(task->filesystems);

    remove_task_from_map(&global_task_map, task);
    free(task);
}

int add_filesystem_to_task(struct fs_task *task, struct Filesystem *fs)
{
    if (task == NULL || fs == NULL)
        return -EINVAL;

    if (task->filesystems_count == task->filesystems_capacity) {
        size_t new_capacity = (task->filesystems_capacity == 0) ? FILESYSTEMS_INITIAL_CAPACITY : task->filesystems_capacity * 2;
        struct Filesystem **new_filesystems = (struct Filesystem **)realloc(task->filesystems, new_capacity * sizeof(struct Filesystem *));
        if (new_filesystems == NULL)
            return -errno;
        
        task->filesystems = new_filesystems;
        task->filesystems_capacity = new_capacity;
    }

    // Binary search for the right place to insert the filesystem
    size_t left = 0;
    size_t right = task->filesystems_count;
    while (left < right) {
        size_t middle = (left + right) / 2;
        if (task->filesystems[middle]->id < fs->id)
            left = middle + 1;
        else
            right = middle;
    }

    // Check if the filesystem with the same id already exists
    if (left < task->filesystems_count && task->filesystems[left]->id == fs->id)
        return -EEXIST;

    // Insert the filesystem
    memmove(task->filesystems + left + 1, task->filesystems + left, (task->filesystems_count - left) * sizeof(struct Filesystem *));
    task->filesystems[left] = fs;
    task->filesystems_count++;

    return 0;
}

void remove_filesystem_from_task(struct fs_task *task, struct Filesystem *fs)
{
    if (task == NULL || fs == NULL)
        return;

    // Binary search for the filesystem
    size_t left = 0;
    size_t right = task->filesystems_count;
    while (left < right) {
        size_t middle = (left + right) / 2;
        if (task->filesystems[middle]->id < fs->id)
            left = middle + 1;
        else
            right = middle;
    }

    // Check if the filesystem with the same id exists
    if (left >= task->filesystems_count || task->filesystems[left]->id != fs->id)
        return;

    // Remove the filesystem
    memmove(task->filesystems + left, task->filesystems + left + 1, (task->filesystems_count - left - 1) * sizeof(struct Filesystem *));
    task->filesystems_count--;

    // Shrink the array if it's too big
    if (task->filesystems_count * 4 < task->filesystems_capacity && task->filesystems_capacity > FILESYSTEMS_INITIAL_CAPACITY) {
        size_t new_capacity = task->filesystems_capacity / 2;
        struct Filesystem **new_filesystems = (struct Filesystem **)realloc(task->filesystems, new_capacity * sizeof(struct Filesystem *));
        if (new_filesystems != NULL) {
            task->filesystems = new_filesystems;
            task->filesystems_capacity = new_capacity;
        }
    }
}

int register_task_with_filesystem(struct Filesystem *fs, struct fs_task *task)
{
    int result = add_task_to_filesystem(fs, task);
    if (result != 0)
        return result;

    result = add_filesystem_to_task(task, fs);
    if (result != 0) {
        remove_task_from_filesystem(fs, task);
        return result;
    }

    return 0;
}

void unregister_task_from_filesystem(struct Filesystem *fs, struct fs_task *task)
{
    if (fs == NULL || task == NULL)
        return;

    remove_task_from_filesystem(fs, task);
    remove_filesystem_from_task(task, fs);
}

static int register_fs_reply(uint64_t reply_port, int result, uint64_t filesystem_id)
{
    IPC_Register_FS_Reply reply = {
        .type = IPC_Register_FS_Reply_NUM,
        .result_code = result,
        .filesystem_id = filesystem_id,
    };

    return -send_message_port(reply_port, sizeof(reply), (char *)&reply);
}

int register_fs(IPC_Register_FS *msg, uint64_t sender, size_t size)
{
    if (size < sizeof(IPC_Register_FS))
        return -EINVAL;

    bool new_task = false;
    struct fs_task *task = find_task_in_map(&global_task_map, sender);
    if (task == NULL) {
        task = create_fs_task(sender);
        if (task == NULL)
            return register_fs_reply(msg->reply_port, -errno, 0);
        new_task = true;
    }


    size_t name_length = size - sizeof(IPC_Register_FS);
    struct Filesystem *fs = create_filesystem(msg->name, name_length);
    if (fs == NULL) {
        if (new_task)
            destroy_fs_task(task);
        return register_fs_reply(msg->reply_port, -errno, 0);
    }

    fs->command_port = msg->fs_port;

    int result = register_task_with_filesystem(fs, task);
    if (result != 0) {
        destroy_filesystem(fs);
        if (new_task)
            destroy_fs_task(task);
        return register_fs_reply(msg->reply_port, result, 0);
    }
    
    int reply_result = register_fs_reply(msg->reply_port, 0, fs->id);
    if (reply_result != 0) {
        unregister_task_from_filesystem(fs, task);
        destroy_filesystem(fs);
        if (new_task)
            destroy_fs_task(task);
        return reply_result;
    }

    return 0;
}

int add_task_to_map(struct fs_task_map *map, struct fs_task *task)
{
    if (map == NULL || task == NULL)
        return -EINVAL;

    if (map->tasks_count == 0 || map->tasks_count > map->tasks_capacity * TASKS_LOAD_FACTOR) {
        size_t new_capacity = (map->tasks_capacity == 0) ? TASKS_INITIAL_CAPACITY : map->tasks_capacity * TASKS_CAPACITY_MULTIPLIER;
        struct fs_task **new_tasks = (struct fs_task **)calloc(sizeof(struct fs_task *), new_capacity);
        if (new_tasks == NULL)
            return -errno;

        // Rehash the tasks
        for (size_t i = 0; i < map->tasks_capacity; i++) {
            struct fs_task *task = map->tasks[i];
            if (task != NULL) {
                size_t new_index = task->id % new_capacity;
                while (new_tasks[new_index] != NULL)
                    new_index = (new_index + 1) % new_capacity;
                new_tasks[new_index] = task;
            }
        }

        free(map->tasks);

        map->tasks = new_tasks;
        map->tasks_capacity = new_capacity;
    }

    size_t index = task->id % map->tasks_capacity;
    while (map->tasks[index] != NULL)
        index = (index + 1) % map->tasks_capacity;
    map->tasks[index] = task;
    map->tasks_count++;

    return 0;
}

void remove_task_from_map(struct fs_task_map *map, struct fs_task *task)
{
    if (map == NULL || task == NULL || map->tasks_count == 0)
        return;

    size_t start_index = task->id % map->tasks_capacity;
    size_t index = start_index;
    do {
        if (map->tasks[index] == task) {
            map->tasks[index] = NULL;
            map->tasks_count--;
            break;
        }
        index = (index + 1) % map->tasks_capacity;
    } while (index != start_index);

    if (index == start_index)
        // The task was not found
        return;

    // Shrink the array if it's too big
    if (map->tasks_count * 4 < map->tasks_capacity && map->tasks_capacity > TASKS_INITIAL_CAPACITY) {
        size_t new_capacity = map->tasks_capacity / 2;
        struct fs_task **new_tasks = (struct fs_task **)calloc(sizeof(struct fs_task *), new_capacity);
        if (new_tasks == NULL)
            return;

        // Rehash the tasks
        for (size_t i = 0; i < map->tasks_capacity; i++) {
            struct fs_task *task = map->tasks[i];
            if (task != NULL) {
                size_t new_index = task->id % new_capacity;
                while (new_tasks[new_index] != NULL)
                    new_index = (new_index + 1) % new_capacity;
                new_tasks[new_index] = task;
            }
        }

        free(map->tasks);

        map->tasks = new_tasks;
        map->tasks_capacity = new_capacity;
    }
}

struct fs_task * find_task_in_map(struct fs_task_map *map, uint64_t task_id)
{
    if (map == NULL || map->tasks_count == 0)
        return NULL;

    size_t start_index = task_id % map->tasks_capacity;
    size_t index = start_index;
    do {
        if (map->tasks[index] != NULL && map->tasks[index]->id == task_id)
            return map->tasks[index];
        index = (index + 1) % map->tasks_capacity;
    } while (index != start_index);

    return NULL;
}

struct Filesystem * find_filesystem_in_map(struct filesystem_map *map, uint64_t fs_id)
{
    if (map == NULL || map->filesystems_count == 0)
        return NULL;

    size_t start_index = fs_id % map->filesystems_capacity;
    size_t index = start_index;
    do {
        if (map->filesystems[index] != NULL && map->filesystems[index]->id == fs_id)
            return map->filesystems[index];
        index = (index + 1) % map->filesystems_capacity;
    } while (index != start_index);

    return NULL;
}

int add_filesystem_to_map(struct filesystem_map *map, struct Filesystem *fs)
{
    if (map == NULL || fs == NULL)
        return -EINVAL;

    if (map->filesystems_count == 0 || map->filesystems_count > map->filesystems_capacity * FILESYSTEMS_MAP_LOAD_FACTOR) {
        size_t new_capacity = (map->filesystems_capacity == 0) ? FILESYSTEMS_INITIAL_CAPACITY : map->filesystems_capacity * FILESYSTEMS_MAP_CAPACITY_MULTIPLIER;
        struct Filesystem **new_filesystems = (struct Filesystem **)calloc(sizeof(struct Filesystem *), new_capacity);
        if (new_filesystems == NULL)
            return -errno;

        // Rehash the filesystems
        for (size_t i = 0; i < map->filesystems_capacity; i++) {
            struct Filesystem *fs = map->filesystems[i];
            if (fs != NULL) {
                size_t new_index = fs->id % new_capacity;
                while (new_filesystems[new_index] != NULL)
                    new_index = (new_index + 1) % new_capacity;
                new_filesystems[new_index] = fs;
            }
        }

        free(map->filesystems);

        map->filesystems = new_filesystems;
        map->filesystems_capacity = new_capacity;
    }

    size_t index = fs->id % map->filesystems_capacity;
    while (map->filesystems[index] != NULL)
        index = (index + 1) % map->filesystems_capacity;
    map->filesystems[index] = fs;
    map->filesystems_count++;

    return 0;
}

void remove_filesystem_from_map(struct filesystem_map *map, struct Filesystem *fs)
{
    if (map == NULL || fs == NULL || map->filesystems_count == 0)
        return;

    size_t start_index = fs->id % map->filesystems_capacity;
    size_t index = start_index;
    do {
        if (map->filesystems[index] == fs) {
            map->filesystems[index] = NULL;
            map->filesystems_count--;
            break;
        }
        index = (index + 1) % map->filesystems_capacity;
    } while (index != start_index);

    if (index == start_index)
        // The filesystem was not found
        return;

    // Shrink the array if it's too big
    if (map->filesystems_count * 4 < map->filesystems_capacity && map->filesystems_capacity > FILESYSTEMS_INITIAL_CAPACITY) {
        size_t new_capacity = map->filesystems_capacity / 2;
        struct Filesystem **new_filesystems = (struct Filesystem **)calloc(sizeof(struct Filesystem *), new_capacity);
        if (new_filesystems == NULL)
            return;

        // Rehash the filesystems
        for (size_t i = 0; i < map->filesystems_capacity; i++) {
            struct Filesystem *fs = map->filesystems[i];
            if (fs != NULL) {
                size_t new_index = fs->id % new_capacity;
                while (new_filesystems[new_index] != NULL)
                    new_index = (new_index + 1) % new_capacity;
                new_filesystems[new_index] = fs;
            }
        }

        free(map->filesystems);

        map->filesystems = new_filesystems;
        map->filesystems_capacity = new_capacity;
    }
}