#pragma once
#include "pair.hh"
#include "../memory/malloc.hh"

namespace klib {

template<typename K>
class set {
private:
    struct node {
        K key;
        node* left = &NIL;
        node* right = &NIL;
        node* parent = &NIL;
        bool red = false;
    };

    size_t elements;
    node* root;
    constexpr static node NIL = {{}, nullptr, nullptr, nullptr, false};

    void erase_node(node*);
    void rotate_left(node*);
    void rotate_right(node*);
    void transplant_node(node*,node*);
    void erase_fix(node*);
    node* search(const K&);
    node* min(node*);
public:
    constexpr set():
        elements(0), root(&NIL) {};
    set(const set&);
    set(set&&);

    set& operator=(const set&);
    set& operator=(set&&);
    ~set();

    bool empty() const noexcept
    {
        return elements == 0;
    }

    size_t size() const noexcept
    {
        return elements;
    }

    void insert(const K&);
    void insert(K&&);
    void emplace(K&&);

    void erase(const K&);

    void swap(set&);
    void clear();

    size_t count(const K&);
};

template<typename K>
set<K>::~set()
{
    while (root != &NIL)
        erase_node(root);
}

template<typename K>
typename set<K>::node* set<K>::search(const K& key)
{
    node* p = root;

    while (p != &NIL and p->key != key) {
        if (p->key > key) 
            p = p->left;
        else
            p = p->right;
    }

    return p;
}

template<typename K>
typename set<K>::node* set<K>::min(node* n)
{
    while (n->left != &NIL)
        n = n->left;

    return n;
}

template<typename K>
size_t set<K>::count(const K& key)
{
    node* p = search(key);
    return p == &NIL;
}

template<typename K>
void set<K>::erase(const K& key)
{
    node* p = search(key);
    erase_node(p);
}

template<class K>
void set<K>::rotate_left(set<K>::node* p)
{
    node* n = p->right;
    p->right = n->left;
    if (n->left != &NIL) {
        n->left->parent = p;
    }
    n->parent = p->parent;
    if (n->parent == &NIL) { // p is root
        root = n;
    } else if (p == p->parent->left) { // x is left child
        p->parent->left = n;
    } else { // x is right child
        p->parent->right = n;
    }
    n->left = p;
    p->parent = n;
}

template<class K>
void set<K>::rotate_right(set<K>::node* p)
{
    node* n = p->left;
    p->left = n->right;
    if (n->right != &NIL) {
        n->right->parent = p;
    }
    n->parent = p->parent;
    if (n->parent == &NIL) {
        root = n;
    } else if (p == p->parent->right) {
        p->parent->right = n;
    } else {
        p->parent->left = n;
    }
    n->right = p;
    p->parent = n;
}

template<class K>
void set<K>::transplant_node(node* n, node* p)
{
    if (n->parent == &NIL) {
        root = p;
    } else if (n == n->parent->left) {
        n->parent->left = p;
    } else {
        n->parent->right = p;
    }

    p->parent = n->parent;
}

template<class K>
void set<K>::erase_node(node* n)
{
    node* y = n;
    bool original_red = n->red;
    node* x;

    if (n->left == &NIL) {
        x = n->right;
        transplant_node(n, n->right);
    } else if (n->right == &NIL) {
        x = n->left;
        transplant_node(n, n->left);
    } else {
        y = min(n->right);
        original_red = y->red;
        x = y->right;
        if (y->parent == n)
            x->parent = y;
        else {
            transplant_node(y, y->right);
            y->right = n->right;
            y->right->parent = y;
        }
        transplant_node(n, y);
        y->left = n->left;
        y->left->parent = y;
        y->red = n->red;
    }

    delete n;

    if (not original_red)
        erase_fix(x);
}

template<class K>
void set<K>::erase_fix(node* x)
{
    while (x != root and not x->red) {
        if (x == x->parent->left) {
            node* w = x->parent->right;
            if (w->red) {
                w->red = false;
                x->parent->red = true;
                rotate_left(x->parent);
                w = x->parent->right;
            }

            if (not w->left->red and not w->right->red) {
                w->red = true;
                x = x->parent;
            } else {
                if (not w->right->red) {
                    w->left->red = false;
                    w->red = true;
                    rotate_right(w);
                    w = x->parent->right;
                }

                w->red = x->parent->red;
                x->parent->red = false;
                w->right->red = false;
                rotate_left(x->parent);
                x = root;
            }
        } else {
            node* w = x->parent->left;
            if (w->red) {
                w->red = false;
                x->parent->red = true;
                rotate_right(x->parent);
                w = x->parent->left;
            }

            if (not w->left->red and not w->right->red) {
                w->red = true;
                x = x->parent;
            } else {
                if (not w->left->red) {
                    w->right->red = false;
                    w->red = true;
                }

                w->red = x->parent->red;
                x->parent->red = false;
                w->left->red = false;
                rotate_right(x->parent);
                x = root;
            }
        }
    }
    x->red = false;
}

}
