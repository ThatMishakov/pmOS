#ifndef PATH_NODE_H
#define PATH_NODE_H
#include <stdint.h>
#include <stddef.h>
#include "file_descriptor.h"

typedef struct Path_Node {
    long height;
    
    struct Path_Node *tree_parent;
    struct Path_Node *tree_left;
    struct Path_Node *tree_right;

    struct File_Descriptor *parent;
    struct File_Descriptor *file_desc;

    size_t name_length;
    unsigned char name[0];
} Path_Node;

extern Path_Node root;

/**
 * @brief Insert a node into the tree.
 * 
 * This function inserts a new node into the tree cheking for duplicates.
 * 
 * @param root Pointer to the pointer of the root of the tree
 * @param new_node Node to be inserted
 * @return 0 on success. -1 on error (if the duplicate node was found).
 */
int path_node_insert(struct Path_Node **root, struct Path_Node *new_node);

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