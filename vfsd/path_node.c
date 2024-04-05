/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "path_node.h"
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include "file_op.h"
#include "filesystem.h"
#include <assert.h>
#include <string.h>
#include <alloca.h>
#include <assert.h>

extern pmos_port_t main_port;

size_t sdbm_hash(const unsigned char * str, size_t length) {
    size_t hash = 0;
    for (size_t i = 0; i < length; i++) {
        hash = str[i] + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

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

void delete_node_from_linked_list(struct Path_Hash_Vector *vector, struct Path_Node *node) {
    if (node->ll_previous == NULL) {
        assert(node == vector->head);
        vector->head = node->ll_next;
    } else {
        assert(node != vector->head && vector->head != NULL);
        node->ll_previous->ll_next = node->ll_next;
    }

    if (node->ll_next == NULL) {
        assert(node == vector->tail);
        vector->tail = node->ll_previous;
    } else {
        assert(node != vector->tail && vector->tail != NULL);
        node->ll_next->ll_previous = node->ll_previous;
    }

    vector->nodes_count--;
}

int insert_node(struct Path_Hash_Map *map, struct Path_Node *node) {
    if (map == NULL || node == NULL) {
        return -1;
    }

    bool rehashing_failed = false;

    // Check if resizing is needed
    if (map->nodes_count >= map->vector_size * PATH_NODE_HASH_LOAD_FACTOR) {
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

void path_node_delete(struct Path_Hash_Map *map, struct Path_Node *node)
{
    if (map == NULL || node == NULL) {
        return;
    }

    size_t index = sdbm_hash(node->name, node->name_length) % map->vector_size;
    struct Path_Hash_Vector *vector = &(map->hash_vector[index]);

    delete_node_from_linked_list(vector, node);
    map->nodes_count--;

    if (map->vector_size > PATH_NODE_HASH_INITIAL_SIZE && map->nodes_count < map->vector_size * PATH_NODE_HASH_LOAD_FACTOR / 2) {
        // Resize the hash table
        size_t new_vector_size = map->vector_size / 2;
        struct Path_Hash_Vector *new_hash_vector = calloc(new_vector_size, sizeof(struct Path_Hash_Vector));
        if (new_hash_vector == NULL) {
            return;
        }

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

Path_Node root = {
    .parent = &root,
    .name_length = 1,
    .name = "/",
    // Root is a special case: it is always assumed it is mounted. If it is not, it is "unresolved". This allows
    // to processes to asynchronously fire open requests before the root filesystem has been initialized and mounted.
    .file_type = NODE_UNRESOLVED,
};

static struct Path_Node *find_leaf(struct Path_Node *node)
{
    if (node == NULL) {
        return NULL;
    }

    struct Path_Node *current_node = node;
    while (current_node->children_nodes_map != NULL) {
        struct Path_Node *next_node = path_node_get_front_child(current_node);
        if (next_node == NULL) {
            break;
        }
        current_node = next_node;
    }

    return current_node;
}

/**
 * @brief Destroy a leaf path node
 * 
 * This function destroys a node that is a leaf in the path tree. It does not
 * destroy any of its children, if it has them. If the parent node is not null
 * or self, it removes the node from the parent's children map.
 * 
 * @param node Node to be destroyed
 */
static void destroy_path_node_leaf(struct Path_Node *node, int error)
{
    if (node == NULL)
        return;

    if (node->parent != NULL && node->parent != node) {
        // Remove the node from the parent's children map
        path_node_delete(node->parent->children_nodes_map, node);
        node->parent = NULL;
    }

    if (node->children_nodes_map != NULL) {
        // Free the children map
        destroy_path_node_map(node->children_nodes_map);
        node->children_nodes_map = NULL;
    }

    // Free the requests
    while (node->requests_head != NULL) {
        fail_and_destroy_request(node->requests_head, error);
    }

    if (node->owner_mountpoint != NULL && node->owner_mountpoint->node == node) {
        destroy_mountpoint(node->owner_mountpoint, false);
        node->owner_mountpoint = NULL;
    }

    if (node != &root) {
        // Free the node
        free(node);
    }
}

 void destroy_path_node(struct Path_Node *node)
 {
    if (node == NULL) {
        return;
    }

    // The tree can be very high, so do it iteratively
    struct Path_Node *current_node = find_leaf(node);

    while (current_node != node) {
        struct Path_Node *parent_node = current_node->parent;

        destroy_path_node_leaf(current_node, -EAGAIN);

        current_node = find_leaf(parent_node);
    }

    // At this point, node is a leaf
    destroy_path_node_leaf(node, -EAGAIN);
 }

void destroy_path_node_with_error(struct Path_Node *node, int error_code)
{
    if (node == NULL) {
        return;
    }

    // The tree can be very high, so do it iteratively
    struct Path_Node *current_node = find_leaf(node);

    while (current_node != node) {
        struct Path_Node *parent_node = current_node->parent;

        destroy_path_node_leaf(current_node, error_code);

        current_node = find_leaf(parent_node);
    }

    // At this point, node is a leaf
    destroy_path_node_leaf(node, error_code);
}

 struct Path_Node *path_node_get_front_child(struct Path_Node *node)
 {
    if (node == NULL)
        return NULL;

    if (node->children_nodes_map == NULL)
        return NULL;

    if (node->children_nodes_map->nodes_count == 0)
        return NULL;

    for (size_t i = 0 ; i < node->children_nodes_map->vector_size ; i++) {
        struct Path_Node *child = node->children_nodes_map->hash_vector[i].head;
        if (child != NULL) {
            return child;
        }
    }

    // Should never happen but just in case
    return NULL;
 }

void destroy_path_node_map(struct Path_Hash_Map *map)
{
    if (map == NULL) {
        return;
    }

    free(map->hash_vector);
    free(map);
}

void remove_request_from_path_node(struct File_Request *request)
{
    if (request == NULL)
        return;

    if (request->active_node == NULL)
        return;

    struct Path_Node *node = request->active_node;

    if (node->requests_head == request) {
        node->requests_head = request->path_node_next;
    } else {
        request->path_node_prev->path_node_next = request->path_node_next;
    }

    if (node->requests_tail == request) {
        node->requests_tail = request->path_node_prev;
    } else {
        request->path_node_next->path_node_prev = request->path_node_prev;
    }

    request->active_node = NULL;
}

int path_node_resolve_child(struct Path_Node **node, struct Path_Node *parent, const unsigned char *name, size_t name_length)
{
    if (node == NULL || parent == NULL || name == NULL || name_length == 0)
        return -EINVAL;

    assert(parent->file_type == NODE_DIRECTORY);

    struct Path_Node *child = path_map_find(parent->children_nodes_map, name, name_length);
    if (child != NULL) {
        *node = child;
        return 0;
    }

    // The child does not exist, create it
    child = calloc(sizeof(struct Path_Node) + name_length, 1);
    if (child == NULL)
        return -ENOMEM;

    child->file_type = NODE_UNRESOLVED;
    child->owner_mountpoint = parent->owner_mountpoint;

    memcpy(child->name, name, name_length);
    child->name_length = name_length;

    int result = path_node_add_child(parent, child);
    if (result != 0) {
        free(child);
        return result;
    }

    // Send the request to the filesystem
    struct File_Request *request = create_resolve_path_request(child);
    if (request == NULL) {
        destroy_path_node(child);
        return -ENOMEM;
    }

    size_t message_size = sizeof(IPC_FS_Resolve_Path) + name_length;
    IPC_FS_Resolve_Path *message = alloca(message_size);
    
    message->type = IPC_FS_Resolve_Path_NUM,
    message->flags = 0,
    message->reply_port = main_port,
    message->filesystem_id = parent->owner_mountpoint->fs->id,
    message->parent_dir_id = parent->file_id,
    message->operation_id = request->request_id,
    memcpy(message->path_name, name, name_length);

    result = -send_message_port(parent->owner_mountpoint->fs->command_port, message_size, (char *)message);
    if (result != 0) {
        fail_and_destroy_request(request, result);
        destroy_path_node(child);
        return result;
    }

    *node = child;
    return 0;
}

int path_node_add_child(struct Path_Node *parent, struct Path_Node *child)
{
    if (parent == NULL || child == NULL)
        return -EINVAL;

    assert(child->parent == NULL);

    if (parent->children_nodes_map == NULL) {
        parent->children_nodes_map = create_path_map();
        if (parent->children_nodes_map == NULL)
            return -ENOMEM;
    }

    int result = insert_node(parent->children_nodes_map, child);
    if (result >= 0)
        child->parent = parent;

    return result;
}

struct Path_Hash_Map *create_path_map()
{
    struct Path_Hash_Map *new_map = malloc(sizeof(struct Path_Hash_Map));
    if (new_map == NULL)
        return NULL;

    new_map->nodes_count = 0;
    new_map->vector_size = PATH_NODE_HASH_INITIAL_SIZE;
    new_map->hash_vector = calloc(new_map->vector_size, sizeof(struct Path_Hash_Vector));
    if (new_map->hash_vector == NULL) {
        free(new_map);
        return NULL;
    }

    return new_map;
}

struct Path_Node *path_map_find(struct Path_Hash_Map *map, const unsigned char *name, size_t name_length)
{
    if (map == NULL || name == NULL || name_length == 0)
        return NULL;

    if (map->vector_size == 0)
        return NULL;

    size_t index = sdbm_hash(name, name_length) % map->vector_size;
    Path_Node * p = map->hash_vector[index].head;
    while (p != NULL) {
        if (p->name_length == name_length && memcmp(p->name, name, name_length) == 0) {
            return p;
        }

        p = p->ll_next;
    }

    return NULL;
}

bool is_file_type_valid(int type)
{
    switch (type) {
        case NODE_FILE:
        case NODE_DIRECTORY:
            return true;
        default:
            return false;
    }
}