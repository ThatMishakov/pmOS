#include "fs_consumer.h"
#include "filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <pmos/system.h>
#include <pmos/ipc.h>
#include <errno.h>

int register_global_fs_consumer(struct fs_consumer *fs_consumer);

struct fs_consumer * create_fs_consumer()
{
    struct fs_consumer *fs_consumer = malloc(sizeof(struct fs_consumer));
    if (fs_consumer == NULL) {
        // Could not allocate memory
        return NULL;
    }

    // Allocate consumer tasks set
    fs_consumer->consumer_tasks = malloc(sizeof(struct consumer_task *) * CONSUMER_TASKS_INITIAL_CAPACITY);
    if (fs_consumer->consumer_tasks == NULL) {
        // Could not allocate memory
        free(fs_consumer);
        return NULL;
    }

    fs_consumer->consumer_tasks_capacity = CONSUMER_TASKS_INITIAL_CAPACITY;
    fs_consumer->consumer_tasks_count = 0;

    // Initialize the list of filesystems with open files
    fs_consumer->open_filesystem = calloc(sizeof(struct open_filesystem *), OPEN_FILESYSTEM_INITIAL_SIZE);
    if (fs_consumer->open_filesystem == NULL) {
        // Could not allocate memory
        free(fs_consumer->consumer_tasks);
        free(fs_consumer);
        return NULL;
    }

    int result = init_null_string(&fs_consumer->path);
    if (result != 0) {
        // Could not allocate memory
        free(fs_consumer->consumer_tasks);
        free(fs_consumer->open_filesystem);
        free(fs_consumer);
        return NULL;
    }

    fs_consumer->open_filesystem_size = OPEN_FILESYSTEM_INITIAL_SIZE;
    fs_consumer->open_filesystem_count = 0;

    fs_consumer->requests_head = NULL;
    fs_consumer->requests_tail = NULL;
    fs_consumer->requests_count = 0;


    static uint64_t next_consumer_id = 1;

    // Assign a unique ID to the consumer
    fs_consumer->id = __atomic_fetch_add(&next_consumer_id, 1, __ATOMIC_SEQ_CST);

    result = register_global_fs_consumer(fs_consumer);
    if (result != 0) {
        // Could not register the consumer
        free(fs_consumer->consumer_tasks);
        free(fs_consumer->open_filesystem);
        destroy_string(&fs_consumer->path);
        free(fs_consumer);
        return NULL;
    }

    return fs_consumer;
}

void free_buffers_fs_consumer(struct fs_consumer *fs_consumer)
{
    if (fs_consumer->consumer_tasks != NULL)
        free(fs_consumer->consumer_tasks);

    if (fs_consumer->open_filesystem != NULL)
        free(fs_consumer->open_filesystem);

    destroy_string(&fs_consumer->path);
}

// This code is problematic because of potential divisions by zero
int reference_open_filesystem(struct fs_consumer *fs_consumer, struct Filesystem *fs, uint64_t open_count)
{
    if (fs_consumer == NULL || fs == NULL)
        return -1;

    // Find the filesystem in the open map
    size_t index = fs->id % fs_consumer->open_filesystem_size;
    struct consumer_fs_map_node *node = fs_consumer->open_filesystem[index];
    while (node != NULL && node->fs != fs)
        node = node->next;

    if (node != NULL) {
        // Found the filesystem, increment the open count
        node->open_files_count += open_count;
        return 0;
    }

    // The filesystem was not found, add it to the map

    // Resize the hash table as needed
    if (fs_consumer->open_filesystem_count > fs_consumer->open_filesystem_size * OPEN_FILESYSTEM_MAX_LOAD_FACTOR) {
        // Resize the hash table
        size_t new_size = fs_consumer->open_filesystem_size * OPEN_FILESYSTEM_SIZE_MULTIPLIER;
        struct consumer_fs_map_node **new_open_filesystem = malloc(sizeof(struct consumer_fs_map_node *) * new_size);
        if (new_open_filesystem == NULL) {
            // Could not allocate memory
            return -1;
        }

        // Rehash the filesystems
        for (size_t i = 0; i < fs_consumer->open_filesystem_size; i++) {
            node = fs_consumer->open_filesystem[i];
            while (node != NULL) {
                struct consumer_fs_map_node *next = node->next;

                // Rehash the filesystem
                size_t new_index = node->fs->id % new_size;
                node->next = new_open_filesystem[new_index];
                new_open_filesystem[new_index] = node;

                node = next;
            }
        }

        free(fs_consumer->open_filesystem);

        fs_consumer->open_filesystem = new_open_filesystem;
        fs_consumer->open_filesystem_size = new_size;

        index = fs->id % fs_consumer->open_filesystem_size;
    }

    // Allocate a new node
    node = malloc(sizeof(struct consumer_fs_map_node));
    if (node == NULL) {
        // Could not allocate memory
        return -1;
    }

    node->fs = fs;
    node->open_files_count = open_count;
    
    // Add the node to the hash table
    node->next = fs_consumer->open_filesystem[index];
    fs_consumer->open_filesystem[index] = node;
    fs_consumer->open_filesystem_count++;

    return 0;
}

bool is_fs_consumer(struct fs_consumer *fs_consumer, uint64_t id)
{
    if (fs_consumer == NULL)
        return false;

    // Binary search consumer tasks
    size_t left = 0;
    size_t right = fs_consumer->consumer_tasks_count;
    while (left < right) {
        size_t mid = (left + right) / 2;
        if (fs_consumer->consumer_tasks[mid]->task_id < id)
            left = mid + 1;
        else
            right = mid;
    }

    return left < fs_consumer->consumer_tasks_count && fs_consumer->consumer_tasks[left]->task_id == id;
}

struct fs_consumer_map global_fs_consumers = {
    NULL,
    0,
    0,
};

__attribute__((constructor)) void init_global_fs_consumers()
{
    const uint64_t initial_size = FS_CONSUMER_INITIAL_SIZE;

    global_fs_consumers.table = calloc(sizeof(struct fs_consumer_node *), initial_size);
    if (global_fs_consumers.table == NULL) {
        // Could not allocate memory
        exit(1);
    }

    global_fs_consumers.size = initial_size;
}

struct fs_consumer *get_fs_consumer(uint64_t consumer_id)
{
    if (global_fs_consumers.table == NULL)
        return NULL;

    uint64_t hash = consumer_id % global_fs_consumers.size;

    struct fs_consumer_node *curr_node = global_fs_consumers.table[hash];
    while (curr_node != NULL && curr_node->fs_consumer->id != consumer_id)
        curr_node = curr_node->next;

    return curr_node == NULL ? NULL : curr_node->fs_consumer;
}

static int send_create_consumer_reply(pmos_port_t port, uint32_t flags, int32_t result, uint64_t consumer_id)
{
    IPC_Create_Consumer_Reply reply = {
        .type = IPC_Create_Consumer_Reply_NUM,
        .flags = flags,
        .result_code = result,
        .consumer_id = consumer_id,
    };

    return -send_message_port(port, sizeof(IPC_Create_Consumer_Reply), (const char *)&reply);
}

/// Registers a consumer task in the global map
int register_global_fs_consumer(struct fs_consumer *fs_consumer)
{
    // Resize the hash table as needed
    if (global_fs_consumers.count >= global_fs_consumers.size * FS_CONSUMER_MAX_LOAD_FACTOR) {
        // Resize the hash table
        size_t new_size = global_fs_consumers.size > 0 ? global_fs_consumers.size * FS_CONSUMER_SIZE_MULTIPLIER : FS_CONSUMER_INITIAL_SIZE;
        struct fs_consumer_node **new_table = calloc(sizeof(struct fs_consumer_node *), new_size);
        if (new_table == NULL) {
            // Could not allocate memory
            return -ENOMEM;
        }

        // Rehash the consumer tasks
        for (size_t i = 0; i < global_fs_consumers.size; i++) {
            struct fs_consumer_node *node = global_fs_consumers.table[i];
            while (node != NULL) {
                struct fs_consumer_node *next = node->next;

                // Rehash the consumer task
                uint64_t new_hash = node->fs_consumer->id % new_size;
                node->next = new_table[new_hash];
                if (node->next != NULL)
                    node->next->prev = node;
                node->prev = NULL;
                new_table[new_hash] = node;

                node = next;
            }
        }

        free(global_fs_consumers.table);

        global_fs_consumers.table = new_table;
        global_fs_consumers.size = new_size;
    }

    // Allocate a new node
    struct fs_consumer_node *node = malloc(sizeof(struct fs_consumer_node));
    if (node == NULL) {
        // Could not allocate memory
        return -ENOMEM;
    }

    uint64_t index = fs_consumer->id % global_fs_consumers.size;

    node->fs_consumer = fs_consumer;
    node->next = global_fs_consumers.table[index];
    node->prev = NULL;
    if (node->next != NULL)
        node->next->prev = node;
    global_fs_consumers.table[index] = node;

    global_fs_consumers.count++;
    return 0;
}

void remove_fs_consumer_global(struct fs_consumer *fs_consumer)
{
    if (global_fs_consumers.table == NULL)
        return;

    uint64_t index = fs_consumer->id % global_fs_consumers.size;

    struct fs_consumer_node *node = global_fs_consumers.table[index];
    while (node != NULL && node->fs_consumer != fs_consumer)
        node = node->next;

    if (node == NULL)
        return;

    if (node->prev == NULL)
        global_fs_consumers.table[index] = node->next;
    else
        node->prev->next = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;

    free(node);

    global_fs_consumers.count--;

    if (global_fs_consumers.count < global_fs_consumers.size * FS_CONSUMER_SHRINK_FACTOR && global_fs_consumers.size > FS_CONSUMER_INITIAL_SIZE) {
        // Shrink the hash table
        size_t new_size = global_fs_consumers.size * FS_CONSUMER_SHRINK_FACTOR;
        struct fs_consumer_node **new_table = calloc(sizeof(struct fs_consumer_node *), new_size);
        if (new_table == NULL) {
            // Could not allocate memory
            return;
        }

        // Rehash the consumer tasks
        for (size_t i = 0; i < global_fs_consumers.size; i++) {
            struct fs_consumer_node *node = global_fs_consumers.table[i];
            while (node != NULL) {
                struct fs_consumer_node *next = node->next;

                // Rehash the consumer task
                uint64_t new_hash = node->fs_consumer->id % new_size;
                node->next = new_table[new_hash];
                if (node->next != NULL)
                    node->next->prev = node;
                node->prev = NULL;
                new_table[new_hash] = node;

                node = next;
            }
        }

        free(global_fs_consumers.table);

        global_fs_consumers.table = new_table;
        global_fs_consumers.size = new_size;
    }
}

int add_task_to_consumer(struct fs_consumer *fs_consumer, struct consumer_task *task)
{
    // Allocate a new node
    struct consumer_task_node *node = malloc(sizeof(struct consumer_task *));
    if (node == NULL) {
        // Could not allocate memory
        return -ENOMEM;
    }

    if (fs_consumer->consumer_tasks_capacity == fs_consumer->consumer_tasks_count) {
        // Resize the consumer tasks array
        size_t new_capacity = fs_consumer->consumer_tasks_capacity * CONSUMER_TASKS_CAPACITY_MULTIPLIER;
        struct consumer_task **new_array = realloc(fs_consumer->consumer_tasks, sizeof(struct consumer_task *) * new_capacity);
        if (new_array == NULL) {
            // Could not allocate memory
            free(node);
            return -ENOMEM;
        }
        fs_consumer->consumer_tasks = new_array;
        fs_consumer->consumer_tasks_capacity = new_capacity;
    }

    size_t left = 0;
    size_t right = fs_consumer->consumer_tasks_count;
    while (left < right) {
        size_t middle = (left + right) / 2;
        if (fs_consumer->consumer_tasks[middle]->task_id < task->task_id)
            left = middle + 1;
        else
            right = middle;
    }

    // Check if the task is already in the consumer
    if (left < fs_consumer->consumer_tasks_count && fs_consumer->consumer_tasks[left]->task_id == task->task_id) {
        free(node);
        return -EEXIST;
    }

    if (left < fs_consumer->consumer_tasks_count)
        memmove(fs_consumer->consumer_tasks + left + 1, fs_consumer->consumer_tasks + left, sizeof(struct consumer_task *) * (fs_consumer->consumer_tasks_count - left));

    fs_consumer->consumer_tasks[left] = task;
    fs_consumer->consumer_tasks_count++;

    return 0;
}

void remove_task_from_consumer(struct consumer_task *task, struct fs_consumer *fs_consumer)
{
    size_t left = 0;
    size_t right = fs_consumer->consumer_tasks_count;
    while (left < right) {
        size_t middle = (left + right) / 2;
        if (fs_consumer->consumer_tasks[middle]->task_id < task->task_id)
            left = middle + 1;
        else
            right = middle;
    }

    if (left < fs_consumer->consumer_tasks_count && fs_consumer->consumer_tasks[left] == task) {
        free(fs_consumer->consumer_tasks[left]);
        if (left < fs_consumer->consumer_tasks_count - 1)
            memmove(fs_consumer->consumer_tasks + left, fs_consumer->consumer_tasks + left + 1, sizeof(struct consumer_task *) * (fs_consumer->consumer_tasks_count - left - 1));
        fs_consumer->consumer_tasks_count--;
    }
}

int add_consumer_to_task(struct fs_consumer *fs_consumer, struct consumer_task *task)
{
    // Allocate a new node
    struct fs_consumer_node *node = malloc(sizeof(struct fs_consumer_node));
    if (node == NULL) {
        // Could not allocate memory
        return -ENOMEM;
    }

    node->fs_consumer = fs_consumer;
    node->next = NULL;
    node->prev = task->consumers_last;

    if (task->consumers_first == NULL)
        task->consumers_first = node;
    else
        task->consumers_last->next = node;
    task->consumers_last = node;

    return 0;
}

void remove_consumer_from_task(struct fs_consumer *fs_consumer, struct consumer_task *task)
{
    struct fs_consumer_node *node = task->consumers_first;
    while (node != NULL) {
        if (node->fs_consumer == fs_consumer) {
            if (node->prev != NULL)
                node->prev->next = node->next;
            else
                task->consumers_first = node->next;

            if (node->next != NULL)
                node->next->prev = node->prev;
            else
                task->consumers_last = node->prev;

            free(node);
            break;
        }

        node = node->next;
    }
}

void destroy_consumer_task(struct consumer_task *task)
{
    // Remove the consumer task from all consumers
    struct fs_consumer_node *node = task->consumers_first;
    while (node != NULL) {
        struct fs_consumer_node *next = node->next;
        remove_task_from_consumer(task, node->fs_consumer);
        free(node);
        node = next;
    }

    // Remove the consumer task from the global consumer tasks
    remove_global_consumer_task(task);

    free(task);
}

void destroy_fs_consumer(struct fs_consumer *fs_consumer)
{
    // Remove the fs consumer from all consumer tasks
    for (size_t i = 0; i < fs_consumer->consumer_tasks_count; i++)
        remove_consumer_from_task(fs_consumer, fs_consumer->consumer_tasks[i]);
    free(fs_consumer->consumer_tasks);

    // Close all open files
    for (size_t i = 0; i < fs_consumer->open_filesystem_size; i++) {
        struct consumer_fs_map_node* node = fs_consumer->open_filesystem[i];
        while (node != NULL) {
            struct consumer_fs_map_node* next = node->next;
            remove_consumer_from_filesystem(node->fs, node->open_files_count, fs_consumer);
            free(node);
            node = next;
        }
    }
    free(fs_consumer->open_filesystem);

    // Remove the fs consumer from the global fs consumers
    remove_fs_consumer_global(fs_consumer);

    free(fs_consumer);
}

int link_consumer_with_task(struct fs_consumer *fs_consumer, struct consumer_task *task)
{
    int result = add_consumer_to_task(fs_consumer, task);
    if (result < 0)
        return result;

    result = add_task_to_consumer(fs_consumer, task);
    if (result < 0) {
        remove_consumer_from_task(fs_consumer, task);
        return result;
    }

    return 0;
}

int create_consumer(const IPC_Create_Consumer *request, uint64_t sender_task_id, size_t)
{
    // Find consumer task
    struct consumer_task *task = get_consumer_task(sender_task_id);

    bool created_new_consumer_task = false;
    if (task == NULL) {
        // Create a new consumer task
        task = malloc(sizeof(struct consumer_task));
        if (task == NULL) {
            // Could not allocate memory
            return send_create_consumer_reply(request->reply_port, request->flags, -ENOMEM, 0);
        }

        task->task_id = sender_task_id;
        task->consumers_first = NULL;
        task->consumers_last = NULL;

        int result = register_global_consumer_task(task);
        if (result < 0) {
            // Could not add consumer task
            free(task);
            return send_create_consumer_reply(request->reply_port, request->flags, result, 0);
        }

        created_new_consumer_task = true;
    }

    // Create a new fs_consumer
    struct fs_consumer *fs_consumer = create_fs_consumer();
    if (fs_consumer == NULL) {
        // Could not allocate memory
        if (created_new_consumer_task) {
            destroy_consumer_task(task);
        }
        return send_create_consumer_reply(request->reply_port, request->flags, -ENOMEM, 0);
    }

    int result = link_consumer_with_task(fs_consumer, task);
    if (result != 0) {
        // Could not add task to fs_consumer
        destroy_fs_consumer(fs_consumer);
        if (created_new_consumer_task) {
            destroy_consumer_task(task);
        }
        return send_create_consumer_reply(request->reply_port, request->flags, result, 0);
    }

    // Send reply
    result = send_create_consumer_reply(request->reply_port, request->flags, 0, fs_consumer->id);
    if (result != 0) {
        // Could not send reply
        destroy_fs_consumer(fs_consumer);
        if (created_new_consumer_task) {
            destroy_consumer_task(task);
        }
    }

    return result;
}

struct consumer_task_map global_consumer_tasks = {
    .table = NULL,
    .table_size = 0,
    .tasks_count = 0,
};

struct consumer_task *get_consumer_task(uint64_t task_id)
{
    if (global_consumer_tasks.table_size == 0)
        return NULL;
    
    
    size_t index = task_id % global_consumer_tasks.table_size;
    struct consumer_task_map_node *node = global_consumer_tasks.table != NULL ? global_consumer_tasks.table[index] : NULL;
    while (node != NULL) {
        if (node->task->task_id == task_id)
            return node->task;
        node = node->next;
    }
    return NULL;
}

void remove_global_consumer_task(struct consumer_task *task)
{
    if (global_consumer_tasks.table_size == 0)
        return;
    
    size_t index = task->task_id % global_consumer_tasks.table_size;
    struct consumer_task_map_node *node = global_consumer_tasks.table != NULL ? global_consumer_tasks.table[index] : NULL;
    struct consumer_task_map_node *prev = NULL;
    while (node != NULL) {
        if (node->task == task) {
            if (prev != NULL)
                prev->next = node->next;
            else
                global_consumer_tasks.table[index] = node->next;
            global_consumer_tasks.tasks_count--;
            free(node);
            break;
        }
        prev = node;
        node = node->next;
    }

    if (global_consumer_tasks.tasks_count >= global_consumer_tasks.table_size * CONSUMER_TASK_SHRINK_THRESHOLD &&
        global_consumer_tasks.table_size > CONSUMER_TASK_INITIAL_SIZE) {
        // Shrink table
        size_t new_table_size = global_consumer_tasks.table_size * CONSUMER_TASK_SHRINK_FACTOR;
        struct consumer_task_map_node **new_table = calloc(sizeof(struct consumer_task_map_node *), new_table_size);
        if (new_table == NULL) {
            // Could not allocate memory
            return;
        }

        for (size_t i = 0; i < global_consumer_tasks.table_size; i++) {
            node = global_consumer_tasks.table[i];
            while (node != NULL) {
                struct consumer_task_map_node *next = node->next;
                size_t new_index = node->task->task_id % new_table_size;
                node->next = new_table[new_index];
                new_table[new_index] = node;
                node = next;
            }
        }

        free(global_consumer_tasks.table);

        global_consumer_tasks.table = new_table;
        global_consumer_tasks.table_size = new_table_size;
    }
}

int register_global_consumer_task(struct consumer_task *task)
{
    if (global_consumer_tasks.table == NULL || global_consumer_tasks.tasks_count > global_consumer_tasks.table_size * CONSUMER_TASK_MAX_LOAD_FACTOR) {
        // Grow table
        size_t new_table_size = global_consumer_tasks.table_size > 0 ? global_consumer_tasks.table_size * FS_CONSUMER_SIZE_MULTIPLIER : CONSUMER_TASK_INITIAL_SIZE;
        struct consumer_task_map_node **new_table = calloc(sizeof(struct consumer_task_map_node *), new_table_size);
        if (new_table == NULL) {
            // Could not allocate memory
            return -ENOMEM;
        }

        for (size_t i = 0; i < global_consumer_tasks.table_size; i++) {
            struct consumer_task_map_node *node = global_consumer_tasks.table[i];
            while (node != NULL) {
                struct consumer_task_map_node *next = node->next;
                size_t new_index = node->task->task_id % new_table_size;
                node->next = new_table[new_index];
                new_table[new_index] = node;
                node = next;
            }
        }

        free(global_consumer_tasks.table);

        global_consumer_tasks.table = new_table;
        global_consumer_tasks.table_size = new_table_size;
    }

    struct consumer_task_map_node *node = malloc(sizeof(struct consumer_task_map_node));
    if (node == NULL) {
        // Could not allocate memory
        return -ENOMEM;
    }

    size_t index = task->task_id % global_consumer_tasks.table_size;
    node->task = task;
    node->next = global_consumer_tasks.table[index];
    global_consumer_tasks.table[index] = node;
    global_consumer_tasks.tasks_count++;

    return 0;
}

void remove_consumer_from_filesystem(struct Filesystem *fs, uint64_t open_files_count, struct fs_consumer *consumer)
{
    // Not yet implemented
    return;
}