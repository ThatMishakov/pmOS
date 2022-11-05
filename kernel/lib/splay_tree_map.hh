#pragma once
#include "pair.hh"

namespace klib {

template<typename K, typename T>
class splay_tree_map {
private:
    struct node {
        node* left = nullptr;
        node* right = nullptr;
        node* parent = nullptr;
        K key;
        T data;

        constexpr node(const K& key, const T& data): left(nullptr), right(nullptr), key(key), data(data) {};
        constexpr node(const Pair<K, T> p): left(nullptr), right(nullptr), key(p.first), data(p.second) {};
    };

    size_t elements;
    mutable node* root;

    void splay(node* n) const;
    void rotate_left(node*) const;
    void rotate_right(node*) const;
public:
    constexpr splay_tree_map():
        elements(0), root(nullptr) {};
    splay_tree_map(const splay_tree_map&);
    ~splay_tree_map();

    void clear();

    void insert(const Pair<K, T>&);

    void erase(const K&);

    size_t size() const;

    size_t count(const K&) const;

    const T& at(const K&) const;
    T& at(const K&);

    K largest() const;
};

template<class K, class T>
void splay_tree_map<K,T>::splay(splay_tree_map<K,T>::node* n) const
{
    while (n->parent != nullptr) {
        if (n->parent == root) {
            if (n == n->parent->left) {
                rotate_right(n->parent);
            } else {
                rotate_left(n->parent);
            }
        } else {
            node* p = n->parent;
            node* g = p->parent;

            if  (n->parent->left == n and p->parent->left == p) {
                rotate_right(g);
                rotate_right(p);
            } else if (n->parent->left == n and p->parent->right == p) {
                rotate_right(p);
                rotate_left(g);
            } else if (n->parent->right == n and p->parent->right == p) {
                rotate_left(g);
                rotate_right(p);
            } else if (n->parent->right == n and p->parent->left == p) {
                rotate_left(p);
                rotate_right(g);
            }
        }
    }
}

template<class K, class T>
void splay_tree_map<K,T>::rotate_left(splay_tree_map<K,T>::node* p) const
{
    node* n = p->right;
    p->right = n->left;
    if (n->left != nullptr) {
        n->left->parent = p;
    }
    n->parent = p->parent;
    if (n->parent == nullptr) { // p is root
        root = n;
    } else if (p == p->parent->left) { // x is left child
        p->parent->left = n;
    } else { // x is right child
        p->parent->right = n;
    }
    n->left = p;
    p->parent = n;
}

template<class K, class T>
void splay_tree_map<K,T>::rotate_right(splay_tree_map<K,T>::node* p) const
{
    node* n = p->left;
    p->left = n->right;
    if (n->right != nullptr) {
        n->right->parent = p;
    }
    n->parent = p->parent;
    if (n->parent == nullptr) {
        root = n;
    } else if (p == p->parent->right) {
        p->parent->right = n;
    } else {
        p->parent->left = n;
    }
    n->right = p;
    p->parent = n;
}

template<class K, class T>
void splay_tree_map<K,T>::insert(const Pair<K, T>& pair)
{
    node *n = nullptr;
    node* temp = root;
    while (temp != nullptr) {
        n = temp;
        if (temp->key > pair.first) {
            temp = temp->left;
        } else if (temp->key < pair.first) {
            temp = temp->right;
        } else {
            splay(n);
            temp->data = pair.second;
            return;
        }
    }

    node* c = new node(pair);

    if (n == nullptr) {
        root = c;
    } else if (pair.first < n->key) {
        n->left = c;
        c->parent = n;
    } else {
        n->right = c;
        c->parent = n;
    }

    splay(c);

    ++elements;
}

template<class K, class T>
size_t splay_tree_map<K,T>::size() const
{
    return elements;
}

template<class K, class T>
size_t splay_tree_map<K,T>::count(const K& key) const
{
    node* n = root;
    while (n != nullptr and n->key != key) {
        if (n->key < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    if (n != nullptr) {
        splay(n);
        return 1;
    }

    return 0;
}

template<class K, class T>
T& splay_tree_map<K,T>::at(const K& key)
{
    node* n = root;

    while (n->key != key) {
        if (n->key < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    splay(n);

    return n->data;
}

}
