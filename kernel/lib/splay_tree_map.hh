#pragma once
#include "pair.hh"

namespace klib {

template<typename K, typename T>
class splay_tree_map {
private:
    struct node {
        node* left = nullptr;
        node* right = nullptr;
        K key;
        T data;

        constexpr node(const K& key, const T& data): left(nullptr), right(nullptr), key(key), data(data) {};
        constexpr node(const Pair<K, T> p): left(nullptr), right(nullptr), key(p.first), data(p.second) {};
    };

    size_t elements;
    mutable node* root;

    static node* splay(node* root, const K& key);
    static node* rotate_left(node*);
    static node* rotate_right(node*);
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
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::splay(splay_tree_map<K,T>::node* root, const K& key)
{
    if (root == nullptr or root->key == key)
        return root;

    while(root->key != key) {
        if (root->key > key) { // Data is right. Rotate left or return
            if (root->right == nullptr) return root;

            root = rotate_left(root);
        } else { // Rotate right or return
            if (root->left == nullptr) return root;

            root = rotate_right(nullptr);
        }
    }

    return root;
}

template<class K, class T>
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::rotate_left(splay_tree_map<K,T>::node* p)
{
    node* n = p->right;
    p->right = n->left;
    n->left = p;
    return n;
}

template<class K, class T>
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::rotate_right(splay_tree_map<K,T>::node* p)
{
    node* n = p->left;
    p->left = n->right;
    n->right = p;
    return n;
}

template<class K, class T>
void splay_tree_map<K,T>::insert(const Pair<K, T>& pair)
{
    node* p = splay(root, pair.first);

    if (p == nullptr) { // Map is empty
        root = new node(pair);
        ++elements;
    } else if (p->key == pair.first) { // Element is already in the map; change T
        p->data = pair.second;
    } else { // Add new element
        node* n = new node(pair);

        if (p->key > pair.first) {
            n->right = p;
            n->left = p->left;
            p->left = nullptr;
        } else {
            n->left = p;
            n->right = p->right;
            p->right = nullptr;
        }
        ++elements;
        root = n;
    }
}

template<class K, class T>
size_t splay_tree_map<K,T>::size() const
{
    return elements;
}

template<class K, class T>
size_t splay_tree_map<K,T>::count(const K& key) const
{
    node* p = splay(root, key);
    if (p == nullptr or p->key != key) return 0;

    return 1;
}

template<class K, class T>
T& splay_tree_map<K,T>::at(const K& key)
{
    node* p = splay(root, key);
    return p->data;
}

}
