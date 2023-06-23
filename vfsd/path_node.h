#ifndef PATH_NODE_H
#define PATH_NODE_H
#include <stdint.h>
#include <stddef.h>
#include "file_descriptor.h"

struct Path_Hash_Map;

/**
 * @brief Enumerates the types of nodes in the file system.
 */
enum Node_Type {
    DIRECTORY,  ///< Represents a directory node.
    FILE,       ///< Represents a file node.
};

/**
 * @brief Represents a node in the virtual file system hierarchy.
 */
typedef struct Path_Node {
    struct Path_Node *ll_next;               ///< Pointer to the next node in the linked list.
    struct Path_Node *ll_previous;           ///< Pointer to the previous node in the linked list.

    struct Path_Node *parent;                ///< Pointer to the parent node.
    struct Filesystem* parent_fs;            ///< Pointer to the filesystem owner of the file.

    uint64_t file_id;                        ///< ID of the file within the filesystem.
    int file_type;                           ///< Type of the node (DIRECTORY or FILE).

    struct Path_Hash_Map *children_nodes_map; ///< Pointer to the hash map of children nodes.

    size_t name_length;                      ///< Length of the node's name.
    unsigned char name[0];                   ///< Name of the node.
} Path_Node;

/**
 * @brief Represents a vector of nodes for a specific hash value in the Path_Hash_Map.
 */
struct Path_Hash_Vector {
    struct Path_Node *head;    ///< Pointer to the head (first) node in the linked list.
    struct Path_Node *tail;    ///< Pointer to the tail (last) node in the linked list.
    size_t nodes_count;        ///< Number of nodes in the vector.
};

struct Path_Hash_Map {
    struct Path_Hash_Vector *hash_vector;
    size_t vector_size;
    size_t nodes_count;
};

extern Path_Node root;

/**
 * @brief Inserts a Path_Node into the Path_Hash_Map.
 *
 * This function inserts a Path_Node into the specified Path_Hash_Map. If the load factor
 * exceeds the threshold, the hash map will be resized and existing nodes will be rehashed.
 *
 * @param map The Path_Hash_Map to insert the node into.
 * @param node The Path_Node to be inserted.
 *
 * @return 0 on success, 1 if rehashing failed due to memory allocation error, -1 if the map or node is NULL.
 */
int insert_node(struct Path_Hash_Map *map, struct Path_Node *node);

/**
 * @brief Inserts a Path_Node into the linked list represented by the Path_Hash_Vector.
 *
 * This function inserts the given node into the linked list represented by the specified Path_Hash_Vector.
 *
 * @param vector The Path_Hash_Vector representing the linked list.
 * @param node The Path_Node to be inserted.
 */
void insert_node_into_linked_list(struct Path_Hash_Vector *vector, struct Path_Node *node);

/**
 * @brief Search for the node in the tree
 * 
 * This function searches for the node with the given *name* in the tree. Name can have any characters (including '\0') and
 * binary comparisons are used when searching. The str_size is used to determine the size and '\0' is not considered a string
 * terminator.
 * 
 * @param root Root node of the tree 
 * @param str_size size of the name to be searched
 * @param name name to be searched
 * @return pointer to the node. If no node was found, NULL is returned.
 */
struct Path_Node *path_node_find(struct Path_Node *root, size_t str_size, const unsigned char * name);

/**
 * @brief Deletes a node from the tree
 * 
 * This function deletes the node from the tree identified by the pointer to the root pointer. This function does not check
 * if the node is part of the tree. This function does not automatically alliberate new_node upon deletion from the tree.
 * 
 * @param root Pointer to the root of the tree from where the node should be deleted
 * @param node Node to be deleted 
 */
void path_node_delete(struct Path_Node **root, struct Path_Node *node);
#endif