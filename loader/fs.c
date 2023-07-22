#include <fs.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem/errors.h>
#include <filesystem/types.h>
#include <pmos/system.h>
#include <pmos/ports.h>
#include <task_list.h>
#include <assert.h>

struct fs_data filesystem_data = {0};

extern uint64_t loader_port;

uint64_t filesystem_id = 0;

uint64_t fs_service_port = 0;

int init_consumer_open_files(struct fs_consumer *consumer)
{
    if (consumer == NULL)
        return -1;

    if (consumer->open_files != NULL)
        return -1;

    consumer->open_files = malloc(sizeof(struct open_file_bucket *) * OPEN_FILE_BUCKET_START_SIZE);
    if (consumer->open_files == NULL)
        return -1;

    consumer->open_files_size = OPEN_FILE_BUCKET_START_SIZE;
    consumer->open_files_count = 0;

    return 0;
}

int insert_open_file(struct fs_consumer *consumer, struct open_file *file)
{
    if (consumer == NULL || file == NULL)
        return -1;

    struct open_file_bucket *bucket = malloc(sizeof(struct open_file_bucket));
    if (bucket == NULL)
        return -1;

    bucket->file = file;
    bucket->next = NULL;

    if (consumer->open_files == NULL) {
        // Initialize open_files
        if (init_consumer_open_files(consumer) < 0) {
            // Failed to initialize open_files
            free(bucket);
            return -1;
        }
    }

    // Grow hash table if needed according to load factor
    if (consumer->open_files_count >= consumer->open_files_size * OPEN_FILE_LOAD_FACTOR) {
        size_t new_size = consumer->open_files_size * OPEN_FILE_GROWTH_FACTOR;
        struct open_file_bucket **new_buckets = malloc(sizeof(struct open_file_bucket *) * new_size);
        if (new_buckets == NULL) {
            free(bucket);
            return -1;
        }

        memset(new_buckets, 0, sizeof(struct open_file_bucket *) * new_size);

        // Rehash all buckets
        for (size_t i = 0; i < consumer->open_files_size; i++) {
            struct open_file_bucket *bucket = consumer->open_files[i];
            while (bucket != NULL) {
                struct open_file_bucket *next = bucket->next;

                size_t new_index = bucket->file->file_id % new_size;
                bucket->next = new_buckets[new_index];
                new_buckets[new_index] = bucket;

                bucket = next;
            }
        }

        free(consumer->open_files);
        consumer->open_files = new_buckets;
        consumer->open_files_size = new_size;
    }

    // Insert bucket into hash table
    size_t index = file->file_id % consumer->open_files_size;
    bucket->next = consumer->open_files[index];
    consumer->open_files[index] = bucket;

    consumer->open_files_count++;

    return 0;
}

int remove_open_file(struct fs_consumer *consumer, struct open_file *file)
{
    if (consumer == NULL || file == NULL)
        return -1;

    if (consumer->open_files == NULL)
        return -1;

    size_t index = file->file_id % consumer->open_files_size;
    struct open_file_bucket *bucket = consumer->open_files[index];
    struct open_file_bucket *prev = NULL;

    while (bucket != NULL) {
        if (bucket->file == file) {
            // Found the bucket
            if (prev == NULL) {
                // Bucket is the first in the list
                consumer->open_files[index] = bucket->next;
            } else {
                // Bucket is not the first in the list
                prev->next = bucket->next;
            }

            free(bucket);
            consumer->open_files_count--;

            // Check if we need to shrink the hash table
            if (consumer->open_files_count < consumer->open_files_size * OPEN_FILE_SHRINK_FACTOR && consumer->open_files_size > OPEN_FILE_BUCKET_START_SIZE) {
                size_t new_size = consumer->open_files_size / OPEN_FILE_GROWTH_FACTOR;
                struct open_file_bucket **new_buckets = malloc(sizeof(struct open_file_bucket *) * new_size);
                if (new_buckets == NULL)
                    // Failed to shrink hash table
                    return 0;

                memset(new_buckets, 0, sizeof(struct open_file_bucket *) * new_size);

                // Rehash all buckets
                for (size_t i = 0; i < consumer->open_files_size; i++) {
                    struct open_file_bucket *bucket = consumer->open_files[i];
                    while (bucket != NULL) {
                        struct open_file_bucket *next = bucket->next;

                        size_t new_index = bucket->file->file_id % new_size;
                        bucket->next = new_buckets[new_index];
                        new_buckets[new_index] = bucket;

                        bucket = next;
                    }
                }

                free(consumer->open_files);
                consumer->open_files = new_buckets;
                consumer->open_files_size = new_size;
            }

            return 0;
        }

        prev = bucket;
        bucket = bucket->next;
    }

    return -1;
}

int add_consumer(struct fs_consumer *consumer, uint64_t task_id)
{
    if (consumer == NULL)
        return -1;

    if (consumer->tasks == NULL) {
        // Initialize tasks
        consumer->tasks = malloc(sizeof(uint64_t) * TASKS_START_SIZE);
        if (consumer->tasks == NULL)
            return -1;

        consumer->task_capacity = TASKS_START_SIZE;
        consumer->task_count = 0;
    }

    // Grow tasks if needed
    if (consumer->task_count >= consumer->task_capacity) {
        size_t new_capacity = consumer->task_capacity * TASKS_GROWTH_FACTOR;
        uint64_t *new_tasks = realloc(consumer->tasks, sizeof(uint64_t) * new_capacity);

        if (new_tasks == NULL)
            return -1;

        consumer->tasks = new_tasks;
        consumer->task_capacity = new_capacity;
    }

    // Insert task ID using binary insertion

    // Search for the first task ID that is greater than the given task ID
    size_t low = 0;
    size_t high = consumer->task_count;
    while (low < high) {
        size_t mid = (low + high) / 2;
        if (consumer->tasks[mid] < task_id)
            low = mid + 1;
        else
            high = mid;
    }

    // Shift all elements after the insertion point to the right and insert the task ID
    memmove(consumer->tasks + low + 1, consumer->tasks + low, sizeof(uint64_t) * (consumer->task_count - low));
    consumer->tasks[low] = task_id;

    consumer->task_count++;

    return 0;
}

bool is_consumer(struct fs_consumer *consumer, uint64_t task_id)
{
    if (consumer == NULL)
        return false;

    if (consumer->tasks == NULL)
        return false;

    // Binary search for task ID
    size_t low = 0;
    size_t high = consumer->task_count;
    while (low < high) {
        size_t mid = (low + high) / 2;
        if (consumer->tasks[mid] < task_id)
            low = mid + 1;
        else
            high = mid;
    }

    return low < consumer->task_count && consumer->tasks[low] == task_id;
}

struct fs_entry *get_file(struct fs_data * data, uint64_t file_id)
{
    if (file_id > data->entries_size)
        return NULL;

    return data->entries[file_id];
}

void consumer_free_buffers(struct fs_consumer *consumer)
{
    if (consumer == NULL)
        return;

    if (consumer->tasks != NULL) {
        free(consumer->tasks);
        consumer->tasks = NULL;
    }

    if (consumer->open_files != NULL) {
        for (size_t i = 0; i < consumer->open_files_size; i++) {
            struct open_file_bucket *bucket = consumer->open_files[i];
            while (bucket != NULL) {
                struct open_file_bucket *next = bucket->next;
                fs_data_remove_open_file(&filesystem_data, bucket->file);
                destroy_open_file(bucket->file);
                free(bucket);
                bucket = next;
            }
        }

        free(consumer->open_files);
        consumer->open_files = NULL;
    }
}

int register_open_request(IPC_FS_Open *msg, IPC_FS_Open_Reply *reply)
{
    if (msg == NULL || reply == NULL)
        return -1;

    reply->operation_id = msg->operation_id;

    // Check if the file exists
    struct fs_entry *file = get_file(&filesystem_data, msg->file_id);
    if (file == NULL) {
        reply->result_code = FS_ERROR_FILE_NOT_FOUND;
        return 0;
    }

    // Check than file is not a directory
    if (file->type == FS_DIRECTORY) {
        reply->result_code = FS_ERROR_FILE_IS_DIRECTORY;
        return 0;
    }

    struct fs_consumer *consumer = get_fs_consumer(&filesystem_data, msg->fs_consumer_id);
    bool new_consumer = false;

    if (consumer == NULL) {
        // Create new consumer
        consumer = malloc(sizeof(struct fs_consumer));
        if (consumer == NULL) {
            reply->result_code = FS_ERROR_OUT_OF_MEMORY;
            return 0;
        }

        memset(consumer, 0, sizeof(struct fs_consumer));

        consumer->id = msg->fs_consumer_id;
        consumer->open_files_size = OPEN_FILE_BUCKET_START_SIZE;
        consumer->open_files = malloc(sizeof(struct open_file_bucket *) * consumer->open_files_size);
        if (consumer->open_files == NULL) {
            free(consumer);
            reply->result_code = FS_ERROR_OUT_OF_MEMORY;
            return 0;
        }

        memset(consumer->open_files, 0, sizeof(struct open_file_bucket *) * consumer->open_files_size);

        new_consumer = true;

        // Add consumer to the hash table
        if (fs_data_add_consumer(&filesystem_data, consumer) != 0) {
            consumer_free_buffers(consumer);
            free(consumer);
            reply->result_code = FS_ERROR_OUT_OF_MEMORY;
            return 0;
        }
    }

    // Allocate a new open_file struct
    struct open_file *open_file = malloc(sizeof(struct open_file));
    if (open_file == NULL) {
        if (new_consumer) {
            fs_data_remove_consumer(&filesystem_data, consumer);
            consumer_free_buffers(consumer);
            free(consumer);
        }

        reply->result_code = FS_ERROR_OUT_OF_MEMORY;
        return 0;
    }

    // Initialize open_file
    open_file->file_id = filesystem_data.next_open_file_id++;
    open_file->seek_pos = 0;
    open_file->file = file;
    open_file->consumer = consumer;

    // Insert open_file into the global open file table
    int result = fs_data_add_open_file(&filesystem_data, open_file);
    if (result != 0) {
        free(open_file);

        if (new_consumer) {
            fs_data_remove_consumer(&filesystem_data, consumer);
            consumer_free_buffers(consumer);
            free(consumer);
        }

        reply->result_code = FS_ERROR_OUT_OF_MEMORY;
        return 0;
    }

    // Insert open_file into the consumer's open file table
    result = insert_open_file(consumer, open_file);
    if (result != 0) {
        fs_data_remove_open_file(&filesystem_data, open_file);
        free(open_file);

        if (new_consumer) {
            fs_data_remove_consumer(&filesystem_data, consumer);
            consumer_free_buffers(consumer);
            free(consumer);
        }

        reply->result_code = FS_ERROR_OUT_OF_MEMORY;
        return 0;
    }

    reply->result_code = FS_SUCCESS;
    reply->file_id = open_file->file_id;
    reply->fs_flags = 0;
    reply->file_port = loader_port;

    return 0;
}

int fs_data_add_open_file(struct fs_data *data, struct open_file *file)
{
    if (data == NULL || file == NULL)
        return -1;

    // Check if the open file table needs to be resized
    if (data->fs_open_files == NULL || data->open_files_table_size * OPEN_FILE_LOAD_FACTOR >= data->open_files_count) {
        size_t new_size = data->open_files_table_size * OPEN_FILE_GROWTH_FACTOR;
        struct open_file_bucket **new_table = malloc(sizeof(struct open_file_bucket *) * new_size);

        if (new_table == NULL)
            return -1;

        memset(new_table, 0, sizeof(struct open_file_bucket *) * new_size);

        // Rehash all open files
        for (size_t i = 0; i < data->open_files_table_size; i++) {
            struct open_file_bucket *bucket = data->fs_open_files[i];
            while (bucket != NULL) {
                struct open_file_bucket *next = bucket->next;

                size_t new_index = bucket->file->file_id % new_size;
                bucket->next = new_table[new_index];
                new_table[new_index] = bucket;

                bucket = next;
            }
        }

        free(data->fs_open_files);
        data->fs_open_files = new_table;
        data->open_files_table_size = new_size;
    }

    // Allocate a bucket for the open file
    struct open_file_bucket *bucket = malloc(sizeof(struct open_file_bucket));
    if (bucket == NULL)
        return -1;

    bucket->file = file;
    
    // Insert the bucket into the table
    uint64_t index = file->file_id % data->open_files_table_size;
    bucket->next = data->fs_open_files[index];
    data->fs_open_files[index] = bucket;

    data->open_files_count++;

    return 0;
}

struct fs_consumer *get_fs_consumer(struct fs_data *data, uint64_t id)
{
    if (data == NULL)
        return NULL;

    uint64_t index = id % data->consumer_table_size;
    struct fs_consumer_bucket *bucket = data->consumers[index];

    while (bucket != NULL) {
        if (bucket->consumer->id == id)
            return bucket->consumer;

        bucket = bucket->next;
    }

    return NULL;
}

int fs_data_add_consumer(struct fs_data *data, struct fs_consumer *consumer)
{
    if (data == NULL || consumer == NULL)
        return -1;

    // Check if the consumer table needs to be resized
    if (data->consumers == NULL || data->consumer_table_size * CONSUMER_LOAD_FACTOR >= data->consumer_count) {
        size_t new_size = data->consumer_table_size * CONSUMER_GROWTH_FACTOR;
        struct fs_consumer_bucket **new_table = malloc(sizeof(struct fs_consumer_bucket *) * new_size);

        if (new_table == NULL)
            return -1;

        memset(new_table, 0, sizeof(struct fs_consumer_bucket *) * new_size);

        // Rehash all consumers
        for (size_t i = 0; i < data->consumer_table_size; i++) {
            struct fs_consumer_bucket *bucket = data->consumers[i];
            while (bucket != NULL) {
                struct fs_consumer_bucket *next = bucket->next;

                size_t new_index = bucket->consumer->id % new_size;
                bucket->next = new_table[new_index];
                new_table[new_index] = bucket;

                bucket = next;
            }
        }

        free(data->consumers);
        data->consumers = new_table;
        data->consumer_table_size = new_size;
    }

    // Allocate a bucket for the consumer
    struct fs_consumer_bucket *bucket = malloc(sizeof(struct fs_consumer_bucket));
    if (bucket == NULL)
        return -1;

    bucket->consumer = consumer;

    // Insert the bucket into the table
    uint64_t index = consumer->id % data->consumer_table_size;
    bucket->next = data->consumers[index];
    data->consumers[index] = bucket;

    data->consumer_count++;

    return 0;
}

void fs_data_remove_open_file(struct fs_data *data, struct open_file *file)
{
    if (data == NULL || file == NULL)
        return;

    uint64_t index = file->file_id % data->open_files_table_size;
    struct open_file_bucket *bucket = data->fs_open_files[index];
    struct open_file_bucket *prev = NULL;

    while (bucket != NULL) {
        if (bucket->file == file) {
            if (prev == NULL)
                data->fs_open_files[index] = bucket->next;
            else
                prev->next = bucket->next;

            free(bucket);
            data->open_files_count--;
            break;
        }

        prev = bucket;
        bucket = bucket->next;
    }

    // Shrink the table if needed
    if (data->open_files_table_size > OPEN_FILE_BUCKET_START_SIZE && data->open_files_table_size * OPEN_FILE_SHRINK_FACTOR >= data->open_files_count) {
        size_t new_size = data->open_files_table_size / OPEN_FILE_GROWTH_FACTOR;
        struct open_file_bucket **new_table = malloc(sizeof(struct open_file_bucket *) * new_size);

        if (new_table == NULL)
            return;

        memset(new_table, 0, sizeof(struct open_file_bucket *) * new_size);

        // Rehash all open files
        for (size_t i = 0; i < data->open_files_table_size; i++) {
            struct open_file_bucket *bucket = data->fs_open_files[i];
            while (bucket != NULL) {
                struct open_file_bucket *next = bucket->next;

                size_t new_index = bucket->file->file_id % new_size;
                bucket->next = new_table[new_index];
                new_table[new_index] = bucket;

                bucket = next;
            }
        }

        free(data->fs_open_files);
        data->fs_open_files = new_table;
        data->open_files_table_size = new_size;
    }
}

void fs_data_remove_consumer(struct fs_data *data, struct fs_consumer *consumer)
{
    if (data == NULL || consumer == NULL)
        return;

    uint64_t index = consumer->id % data->consumer_table_size;
    struct fs_consumer_bucket *bucket = data->consumers[index];
    struct fs_consumer_bucket *prev = NULL;

    while (bucket != NULL) {
        if (bucket->consumer == consumer) {
            if (prev == NULL)
                data->consumers[index] = bucket->next;
            else
                prev->next = bucket->next;

            free(bucket);
            data->consumer_count--;
            break;
        }

        prev = bucket;
        bucket = bucket->next;
    }

    // Shrink the table if needed
    if (data->consumer_table_size > CONSUMER_BUCKET_START_SIZE && data->consumer_table_size * CONSUMER_SHRINK_FACTOR >= data->consumer_count) {
        size_t new_size = data->consumer_table_size / CONSUMER_GROWTH_FACTOR;
        struct fs_consumer_bucket **new_table = malloc(sizeof(struct fs_consumer_bucket *) * new_size);

        if (new_table == NULL)
            return;

        memset(new_table, 0, sizeof(struct fs_consumer_bucket *) * new_size);

        // Rehash all consumers
        for (size_t i = 0; i < data->consumer_table_size; i++) {
            struct fs_consumer_bucket *bucket = data->consumers[i];
            while (bucket != NULL) {
                struct fs_consumer_bucket *next = bucket->next;

                size_t new_index = bucket->consumer->id % new_size;
                bucket->next = new_table[new_index];
                new_table[new_index] = bucket;

                bucket = next;
            }
        }

        free(data->consumers);
        data->consumers = new_table;
        data->consumer_table_size = new_size;
    }
}

void initialize_filesystem(pmos_port_t vfsd_port)
{
    print_str("Loader: Attempting to register the filesystem with the VFS at port ");
    print_hex(vfsd_port);
    print_str("\n");

    int r = init_fs();
    if (r != 0) {
        print_str("Loader: Failed to initialize the filesystem\n");
        return;
    }

    if (fs_service_port == 0) {
        ports_request_t request = create_port(PID_SELF, 0);
        if (request.result != SUCCESS) {
            print_str("Loader: Failed to create a port for the filesystem request: ");
            print_hex(request.result);
            print_str("\n");
            return;
        }
        fs_service_port = request.port;
    }

    const char * const fs_name = "loader_fs";
    size_t message_size = sizeof(struct IPC_Register_FS) + strlen(fs_name);


    IPC_Register_FS * request = alloca(message_size);
    if (request == NULL) {
        print_str("Loader: Failed to allocate a message for the filesystem request\n");
        return;
    }

    request->num = IPC_Register_FS_NUM;
    request->flags = 0;
    request->reply_port = fs_service_port;
    request->fs_port = loader_port;
    memcpy(request->name, fs_name, strlen(fs_name));

    result_t result = send_message_port(vfsd_port, message_size, (char *)request);
    if (result != SUCCESS) {
        print_str("Loader: Failed to send the filesystem request to the VFS: ");
        print_hex(result);
        print_str("\n");
        return;
    }

    Message_Descriptor reply_descr;
    IPC_Generic_Msg * reply_msg;
    result = get_message(&reply_descr, (unsigned char **)&reply_msg, fs_service_port);
    if (result != SUCCESS) {
        print_str("Loader: Failed to receive a reply from the VFS: ");
        print_hex(result);
        print_str("\n");
        return;
    }

    if (reply_msg->type != IPC_Register_FS_Reply_NUM) {
        print_str("Loader: Received an unexpected reply from the VFS: ");
        print_hex(reply_msg->type);
        print_str("\n");
        return;
    }

    IPC_Register_FS_Reply * reply = (IPC_Register_FS_Reply *)reply_msg;
    filesystem_id = reply->filesystem_id;
    print_hex(reply->result_code);
}

int init_fs()
{
    // Init root node
    struct fs_entry *root = malloc(sizeof(struct fs_entry));
    if (root == NULL)
        return -1;

    root->name = "/";
    root->name_size = 1;

    root->type = FS_DIRECTORY;
    root->directory.entries = NULL;
    root->directory.entry_count = 0;
    root->directory.entry_capacity = 0;

    int result = fs_data_push_entry(&filesystem_data, root);
    if (result != 0) {
        free(root);
        return -1;
    }

    // Parse modules and add them to the filesystem
    struct task_list_node *p = modules_list;
    while (p != NULL) {
        const char * path = p->path;
        if (path == NULL)
            path = p->name;

        if (path == NULL) {
            p = p->next;
            continue;
        }

        if (strchr(path, '/') != NULL) {
            // Subdirectories are not supported
            continue;
        }

        struct fs_entry *entry = malloc(sizeof(struct fs_entry));
        if (entry == NULL) {
            fs_data_clear(&filesystem_data);
            return -1;
        }

        entry->type = FS_FILE_REGULAR;

        entry->name = path;
        if (entry->name == NULL) {
            destroy_fs_entry(entry);
            fs_data_clear(&filesystem_data);
            return -1;
        }
        entry->name_size = strlen(path);

        entry->file.file_size = p->mod_ptr->mod_end - p->mod_ptr->mod_start;
        entry->file.data = p->file_virt_addr;
        entry->file.task = p;

        result = fs_data_push_entry(&filesystem_data, entry);
        if (result != 0) {
            destroy_fs_entry(entry);
            fs_data_clear(&filesystem_data);
            return -1;
        }

        // Add the file to the root directory
        result = fs_entry_add_child(root, entry);
        if (result != 0) {
            fs_data_clear(&filesystem_data);
            return -1;
        }

        p = p->next;
    }

    return 0;
}

void destroy_fs_entry(struct fs_entry *entry)
{
    if (entry == NULL)
        return;

    switch (entry->type) {
        case FS_FILE_REGULAR:
            break;
        case FS_DIRECTORY:
            free(entry->directory.entries);
            break;
        default:
            break;
    }
}

int fs_data_push_entry(struct fs_data *data, struct fs_entry *entry)
{
    if (data == NULL || entry == NULL)
        return -1;

    // Check if the entries array needs to be resized
    if (data->entries_size == data->entries_capacity) {
        size_t new_capacity = data->entries_capacity == 0 ? 4 : data->entries_capacity * 2;
        struct fs_entry **new_entries = realloc(data->entries, sizeof(struct fs_entry *) * new_capacity);
        if (new_entries == NULL)
            return -1;

        data->entries = new_entries;
        data->entries_capacity = new_capacity;
    }

    // Insert the entry into the array
    data->entries[data->entries_size++] = entry;

    return 0;
}

int fs_entry_add_child(struct fs_entry *directory, struct fs_entry *entry)
{
    if (directory == NULL || entry == NULL)
        return -1;

    if (directory->type != FS_DIRECTORY)
        return -1;

    // Check if the directory's entries array needs to be resized
    if (directory->directory.entry_count == directory->directory.entry_capacity) {
        size_t new_capacity = directory->directory.entry_capacity == 0 ? 4 : directory->directory.entry_capacity * 2;
        struct fs_entry **new_entries = realloc(directory->directory.entries, sizeof(struct fs_entry *) * new_capacity);
        if (new_entries == NULL)
            return -1;

        directory->directory.entries = new_entries;
        directory->directory.entry_capacity = new_capacity;
    }

    // Insert the entry into the array
    directory->directory.entries[directory->directory.entry_count++] = entry;

    return 0;
}

void fs_data_clear(struct fs_data *data)
{
    if (data == NULL)
        return;

    if (data->entries != NULL) {
        for (size_t i = 0; i < data->entries_size; ++i)
            destroy_fs_entry(data->entries[i]);

        free(data->entries);
        data->entries = NULL;
        data->entries_size = 0;
        data->entries_capacity = 0;
    }

    if (data->consumers != NULL) {
        for (size_t i = 0; i < data->consumer_table_size; ++i) {
            struct fs_consumer_bucket *bucket = data->consumers[i];
            while (bucket != NULL) {
                struct fs_consumer_bucket *next = bucket->next;
                consumer_free_buffers(bucket->consumer);
                free(bucket->consumer);
                free(bucket);
                bucket = next;
            }
        }

        free(data->consumers);
        data->consumers = NULL;
        data->consumer_table_size = 0;
        data->consumer_count = 0;
    }

    if (data->fs_open_files != NULL) {
        for (size_t i = 0; i < data->open_files_table_size; ++i) {
            struct open_file_bucket *bucket = data->fs_open_files[i];
            while (bucket != NULL) {
                struct open_file_bucket *next = bucket->next;
                destroy_open_file(bucket->file);
                free(bucket);
                bucket = next;
            }
        }

        free(data->fs_open_files);
        data->fs_open_files = NULL;
        data->open_files_table_size = 0;
        data->open_files_count = 0;
    }
}

void destroy_open_file(struct open_file *file)
{
    if (file == NULL)
        return;

    free(file);
}