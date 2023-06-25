#include <fs.h>
#include <stdlib.h>
#include <string.h>

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

