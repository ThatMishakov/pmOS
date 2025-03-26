#pragma once
#include <stddef.h>
#include <stdint.h>

#define RBTREE(NAME, T, CMP_FUNC, KEY_CMP_FUNC)                                                   \
    typedef struct NAME##_node {                                                                  \
        T data;                                                                                   \
        struct NAME##_node *parent, *left, *right;                                                \
        int color;                                                                                \
    } NAME##_node_t;                                                                              \
                                                                                                  \
    typedef struct {                                                                              \
        NAME##_node_t *root;                                                                      \
    } NAME##_tree_t;                                                                              \
                                                                                                  \
    enum {                                                                                        \
        NAME##_BLACK = 0,                                                                         \
        NAME##_RED   = 1,                                                                         \
    };                                                                                            \
                                                                                                  \
    static inline void NAME##_rotate_right(NAME##_tree_t *tree, NAME##_node_t *node)              \
    {                                                                                             \
        NAME##_node_t *left = node->left;                                                         \
        node->left          = left->right;                                                        \
        if (node->left)                                                                           \
            node->left->parent = node;                                                            \
        left->parent = node->parent;                                                              \
        if (!node->parent)                                                                        \
            tree->root = left;                                                                    \
        else if (node == node->parent->right)                                                     \
            node->parent->right = left;                                                           \
        else                                                                                      \
            node->parent->left = left;                                                            \
        left->right  = node;                                                                      \
        node->parent = left;                                                                      \
    }                                                                                             \
                                                                                                  \
    static inline void NAME##_rotate_left(NAME##_tree_t *tree, NAME##_node_t *node)               \
    {                                                                                             \
        NAME##_node_t *right = node->right;                                                       \
        node->right          = right->left;                                                       \
        if (right->left)                                                                          \
            node->right->parent = node;                                                           \
        right->parent = node->parent;                                                             \
        if (!node->parent)                                                                        \
            tree->root = right;                                                                   \
        else if (node == node->parent->left)                                                      \
            node->parent->left = right;                                                           \
        else                                                                                      \
            node->parent->right = right;                                                          \
        right->left  = node;                                                                      \
        node->parent = right;                                                                     \
    }                                                                                             \
                                                                                                  \
    static inline void NAME##_fix_insert(NAME##_tree_t *tree, NAME##_node_t *node)                \
    {                                                                                             \
        while (node->parent && node->parent->color == NAME##_RED) {                               \
            NAME##_node_t *parent      = node->parent;                                            \
            NAME##_node_t *grandparent = parent->parent;                                          \
            if (parent == grandparent->left) {                                                    \
                NAME##_node_t *uncle = grandparent->right;                                        \
                if (uncle && uncle->color == NAME##_RED) {                                        \
                    parent->color      = NAME##_BLACK;                                            \
                    uncle->color       = NAME##_BLACK;                                            \
                    grandparent->color = NAME##_RED;                                              \
                    node               = grandparent;                                             \
                } else {                                                                          \
                    if (node == parent->right) {                                                  \
                        node = parent;                                                            \
                        NAME##_rotate_left(tree, node);                                           \
                        parent      = node->parent;                                               \
                        grandparent = parent->parent;                                             \
                    }                                                                             \
                    parent->color      = NAME##_BLACK;                                            \
                    grandparent->color = NAME##_RED;                                              \
                    NAME##_rotate_right(tree, grandparent);                                       \
                }                                                                                 \
            } else {                                                                              \
                NAME##_node_t *uncle = grandparent->left;                                         \
                if (uncle && uncle->color == NAME##_RED) {                                        \
                    parent->color      = NAME##_BLACK;                                            \
                    uncle->color       = NAME##_BLACK;                                            \
                    grandparent->color = NAME##_RED;                                              \
                } else {                                                                          \
                    if (node == parent->left) {                                                   \
                        node = parent;                                                            \
                        NAME##_rotate_right(tree, node);                                          \
                        parent      = node->parent;                                               \
                        grandparent = parent->parent;                                             \
                    }                                                                             \
                    parent->color      = NAME##_BLACK;                                            \
                    grandparent->color = NAME##_RED;                                              \
                    NAME##_rotate_left(tree, grandparent);                                        \
                }                                                                                 \
            }                                                                                     \
        }                                                                                         \
        tree->root->color = NAME##_BLACK;                                                         \
    }                                                                                             \
                                                                                                  \
    static inline void NAME##_insert(NAME##_tree_t *tree, NAME##_node_t *node)                    \
    {                                                                                             \
        node->left = node->right = node->parent = NULL;                                           \
                                                                                                  \
        NAME##_node_t *current = tree->root;                                                      \
        NAME##_node_t *parent  = NULL;                                                            \
        while (current) {                                                                         \
            parent = current;                                                                     \
            if (CMP_FUNC(&node->data, &current->data) < 0)                                        \
                current = current->left;                                                          \
            else                                                                                  \
                current = current->right;                                                         \
        }                                                                                         \
                                                                                                  \
        node->parent = parent;                                                                    \
        node->color  = NAME##_RED;                                                                \
        if (!parent) {                                                                            \
            tree->root  = node;                                                                   \
            node->color = NAME##_BLACK;                                                           \
        } else if (CMP_FUNC(&node->data, &current->data) < 0)                                     \
            parent->left = node;                                                                  \
        else                                                                                      \
            parent->right = node;                                                                 \
                                                                                                  \
        NAME##_fix_insert(tree, node);                                                            \
    }                                                                                             \
                                                                                                  \
    static inline void NAME##_fix_remove(NAME##_tree_t *tree, NAME##_node_t *node,                \
                                         NAME##_node_t *parent)                                   \
    {                                                                                             \
        while (node != tree->root && (!node || node->color == NAME##_BLACK)) {                    \
            if (node == parent->left) {                                                           \
                NAME##_node_t *sibling = parent->right;                                           \
                if (sibling->color == NAME##_RED) {                                               \
                    sibling->color = NAME##_BLACK;                                                \
                    parent->color  = NAME##_RED;                                                  \
                    NAME##_rotate_left(tree, parent);                                             \
                    sibling = parent->right;                                                      \
                }                                                                                 \
                                                                                                  \
                if ((!sibling->left || sibling->left->color == NAME##_BLACK) &&                   \
                    (!sibling->right || sibling->right->color == NAME##_BLACK)) {                 \
                    sibling->color = NAME##_RED;                                                  \
                    node           = parent;                                                      \
                    parent         = parent->parent;                                              \
                } else {                                                                          \
                    if (!sibling->right || sibling->right->color == NAME##_BLACK) {               \
                        sibling->left->color = NAME##_BLACK;                                      \
                        sibling->color       = NAME##_RED;                                        \
                        NAME##_rotate_right(tree, sibling);                                       \
                        sibling = parent->right;                                                  \
                    }                                                                             \
                                                                                                  \
                    sibling->color        = parent->color;                                        \
                    parent->color         = NAME##_BLACK;                                         \
                    sibling->right->color = NAME##_BLACK;                                         \
                    NAME##_rotate_left(tree, parent);                                             \
                    node = tree->root;                                                            \
                }                                                                                 \
            } else {                                                                              \
                NAME##_node_t *sibling = parent->left;                                            \
                if (sibling->color == NAME##_BLACK) {                                             \
                    sibling->color = NAME##_BLACK;                                                \
                    parent->color  = NAME##_RED;                                                  \
                    NAME##_rotate_right(tree, parent);                                            \
                    sibling = parent->left;                                                       \
                }                                                                                 \
                                                                                                  \
                if ((!sibling->right || sibling->right->color == NAME##_BLACK) &&                 \
                    (!sibling->left || sibling->left->color == NAME##_BLACK)) {                   \
                    sibling->color = NAME##_RED;                                                  \
                    node           = parent;                                                      \
                    parent         = node->parent;                                                \
                } else {                                                                          \
                    if (!sibling->left || sibling->left->color == NAME##_BLACK) {                 \
                        sibling->right->color = NAME##_BLACK;                                     \
                        sibling->color        = NAME##_RED;                                       \
                        NAME##_rotate_left(tree, sibling);                                        \
                        sibling = parent->left;                                                   \
                    }                                                                             \
                                                                                                  \
                    sibling->color       = parent->color;                                         \
                    parent->color        = NAME##_BLACK;                                          \
                    sibling->left->color = NAME##_BLACK;                                          \
                    NAME##_rotate_right(tree, parent);                                            \
                    node = tree->root;                                                            \
                }                                                                                 \
            }                                                                                     \
        }                                                                                         \
                                                                                                  \
        if (node)                                                                                 \
            node->color = NAME##_BLACK;                                                           \
    }                                                                                             \
                                                                                                  \
    static inline void NAME##_transplant(NAME##_tree_t *tree, NAME##_node_t *u, NAME##_node_t *v) \
    {                                                                                             \
        if (!u->parent)                                                                           \
            tree->root = v;                                                                       \
        else if (u == u->parent->left)                                                            \
            u->parent->left = v;                                                                  \
        else                                                                                      \
            u->parent->right = v;                                                                 \
                                                                                                  \
        if (v)                                                                                    \
            v->parent = u->parent;                                                                \
    }                                                                                             \
                                                                                                  \
    static inline void NAME##_remove(NAME##_tree_t *tree, NAME##_node_t *node)                    \
    {                                                                                             \
        NAME##_node_t *current = node;                                                            \
        NAME##_node_t *child   = NULL;                                                            \
        NAME##_node_t *parent  = node->parent;                                                    \
        int original_color     = node->color;                                                     \
        if (!node->left) {                                                                        \
            child = current->right;                                                               \
            NAME##_transplant(tree, current, child);                                              \
        } else if (!node->right) {                                                                \
            child = current->left;                                                                \
            NAME##_transplant(tree, node, child);                                                 \
        } else {                                                                                  \
            NAME##_node_t *successor = node->right;                                               \
            while (successor->left) {                                                             \
                successor = successor->left;                                                      \
            }                                                                                     \
                                                                                                  \
            original_color = successor->color;                                                    \
            child          = successor->right;                                                    \
            parent         = successor;                                                           \
            if (successor->parent == current) {                                                   \
                if (child)                                                                        \
                    child->parent = successor;                                                    \
            } else {                                                                              \
                parent = successor->parent;                                                       \
                NAME##_transplant(tree, successor, successor->right);                             \
                successor->right         = current->right;                                        \
                successor->right->parent = successor;                                             \
            }                                                                                     \
                                                                                                  \
            NAME##_transplant(tree, current, successor);                                          \
            successor->left         = current->left;                                              \
            successor->left->parent = successor;                                                  \
            successor->color        = node->color;                                                \
        }                                                                                         \
                                                                                                  \
        if (original_color == NAME##_BLACK)                                                       \
            NAME##_fix_remove(tree, child, parent);                                               \
    }                                                                                             \
                                                                                                  \
    static inline NAME##_node_t *find(const NAME##_tree_t *tree, void *key)                       \
    {                                                                                             \
        NAME##_node_t *n = tree->root;                                                            \
        while (n) {                                                                               \
            int cmp = KEY_CMP_FUNC(&n->data, key);                                                \
            if (cmp < 0)                                                                          \
                n = n->left;                                                                      \
            else if (cmp > 0)                                                                     \
                n = n->right;                                                                     \
            else                                                                                  \
                return n;                                                                         \
        }                                                                                         \
        return NULL;                                                                              \
    }
