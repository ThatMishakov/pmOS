#include "path_node.h"
#include <stdbool.h>

void insert_node_into_linked_list(struct Path_Hash_Vector *vector, struct Path_Node *node) {
    if (vector->head == NULL) {
        vector->head = node;
        vector->tail = node;
    } else {
        node->ll_previous = vector->tail;
        vector->tail->ll_next = node;
        vector->tail = node;
    }

    node->ll_next = NULL;
    vector->nodes_count++;
}

int insert_node(struct Path_Hash_Map *map, struct Path_Node *node) {
    if (map == NULL || node == NULL) {
        return -1;
    }

    bool rehashing_failed = false;

    // Check if resizing is needed
    if (map->nodes_count >= map->vector_size * HASH_LOAD_FACTOR) {
        // Resize the hash table
        size_t new_vector_size = map->vector_size * 2;
        struct Path_Hash_Vector *new_hash_vector = calloc(new_vector_size, sizeof(struct Path_Hash_Vector));
        if (new_hash_vector == NULL) {
            rehashing_failed = true;
        } else {
            // Rehash the existing nodes into the new hash vector
            for (size_t i = 0; i < map->vector_size; i++) {
                struct Path_Node *current_node = map->hash_vector[i].head;
                while (current_node != NULL) {
                    struct Path_Node *next_node = current_node->ll_next;

                    size_t index = sdbm_hash(current_node->name, current_node->name_length) % new_vector_size;
                    struct Path_Hash_Vector *vector = &(new_hash_vector[index]);

                    insert_node_into_linked_list(vector, current_node);
                    current_node = next_node;
                }
            }

            // Free the memory used by the old hash vector
            free(map->hash_vector);

            // Update the hash vector and vector size
            map->hash_vector = new_hash_vector;
            map->vector_size = new_vector_size;
        }
    }

    size_t index = sdbm_hash(node->name, node->name_length) % map->vector_size;
    struct Path_Hash_Vector *vector = &(map->hash_vector[index]);

    insert_node_into_linked_list(vector, node);
    map->nodes_count++;

    if (rehashing_failed) {
        return 1;
    }

    return 0;
}