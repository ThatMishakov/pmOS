#ifndef TREES_H
#define TREES_H


#define rotate_left(root, node, field) \
    { \
        timer_tree* right = node->field.r; \
        node->field.r = right->field.l; \
        if (node->field.r) \
            node->field.r->field.p = node; \
        right->field.p = node->field.p; \
        if (!node->field.p) \
            root = right; \
        else if (node == node->field.p->field.l) \
            node->field.p->field.l = right; \
        else \
            node->field.p->field.r = right; \
        right->field.l = node; \
        node->field.p = right; \
    }

#define rotate_right(root, node, field) \
    { \
        timer_tree* left = node->field.l; \
        node->field.l = left->field.r; \
        if (node->field.l) \
            node->field.l->field.p = node; \
        left->field.p = node->field.p; \
        if (!node->field.p) \
            root = left; \
        else if (node == node->field.p->field.l) \
            node->field.p->field.l = left; \
        else \
            node->field.p->field.r = left; \
        left->field.l = node; \
        node->field.p = left; \
    }

#endif