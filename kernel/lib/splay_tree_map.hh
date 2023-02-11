#pragma once
#include "pair.hh"
#include "../memory/malloc.hh"
#include "utility.hh"
#include "../utils.hh"

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

        node() = default;
        constexpr node(const K& key, const T& data): left(nullptr), right(nullptr), key(key), data(data) {};
        constexpr node(const pair<K, T>& p): left(nullptr), right(nullptr), key(p.first), data(p.second) {};
        constexpr node(pair<K, T>&& p): left(nullptr), right(nullptr), key(forward<T>(p.first)), data(forward<T>(p.second)) {};
    };

    size_t elements;
    mutable node* root;

    void splay(node* n) const;
    void rotate_left(node*) const;
    void rotate_right(node*) const;
    void delete_node(node*);
    static node* max(node*);
    static node* min(node*);
public:
    friend class iterator;
    class iterator {
        friend class splay_tree_map;
    private:
        node* ptr = nullptr;

        constexpr iterator(node* n): ptr(n) {};
    public:
        iterator() = default;
        
        constexpr bool operator==(iterator it) const
        {
            return ptr == it.ptr;
        }

        constexpr pair<const K&, T&> operator*() const
        {
            return {ptr->key, ptr->data};
        }

        constexpr pair<const K* const, T* const> operator->() const
        {
            return {&ptr->key, &ptr->data};
        }
    };

    constexpr splay_tree_map():
        elements(0), root(nullptr) {};
    splay_tree_map(const splay_tree_map&);
    splay_tree_map(splay_tree_map&&);
    ~splay_tree_map();

    void clear();

    void insert(const pair<K, T>&);
    void insert(pair<K, T>&&);

    void erase(const K&);

    constexpr size_t size() const;
    constexpr bool empty() const;

    size_t count(const K&) const;

    const T& at(const K&) const;
    T& at(const K&);

    K largest() const;

    T get_copy_or_default(const K&);

    iterator begin() const
    {
        if (root != nullptr)
            splay(min(root));

        return root;
    }

    constexpr iterator end() const
    {
        return nullptr;
    } 
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

            if  (p->left == n and g->left == p) {
                rotate_right(g);
                rotate_right(p);
            } else if (p->left == n and g->right == p) {
                rotate_right(p);
                rotate_left(g);
            } else if (p->right == n and g->right == p) {
                rotate_left(g);
                rotate_left(p);
            } else if (p->right == n and g->left == p) {
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
void splay_tree_map<K,T>::insert(const pair<K, T>& pair)
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

template<typename K, typename T>
void splay_tree_map<K,T>::insert(pair<K, T>&& pair)
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
            temp->data = forward<T>(pair.second);
            return;
        }
    }

    node* c = new node();
    c->data = forward<T>(pair.second);
    c->key = pair.first;

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
constexpr size_t splay_tree_map<K,T>::size() const
{
    return elements;
}

template<class K, class T>
constexpr bool splay_tree_map<K,T>::empty() const
{
    return size() == 0;
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

template<class K, class T>
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::max(splay_tree_map<K,T>::node* p)
{
    while (p->right != nullptr) {
        p = p->right;
    }
    return p;
}


template<class K, class T>
void splay_tree_map<K,T>::erase(const K& key)
{
    node* n = root;

    while (n->key != key) {
        if (n->key < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    delete_node(n);
}

template<class K, class T>
splay_tree_map<K,T>::~splay_tree_map()
{
    while (root != nullptr) delete_node(root);
}

template<class K, class T>
void splay_tree_map<K,T>::delete_node(splay_tree_map<K,T>::node* p)
{
    splay(p);

    node* left = root->left;
    if (left != nullptr) {
        left->parent = nullptr;
    }

    node* right = root->right;
    if (right != nullptr) {
        right->parent = nullptr;
    }

    delete p;
    --elements;

    if (left != nullptr) {
        root = left;
        node* p = max(root);
        p->right = right;
        if (right != nullptr) right->parent = p;
        splay(p);
    } else {
        root = right;
    }
}

template<class K, class T>
T splay_tree_map<K,T>::get_copy_or_default(const K& key)
{
    node* n = root;

    while (n != nullptr and n->key != key) {
        if (n->key < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    if (n == nullptr) return T();

    splay(n);

    return n->data;
}

template<class K, class T>
K splay_tree_map<K,T>::largest() const
{
    node* n = root;
    while (n->right != nullptr)
        n = n->right;

    return n->key;
}

}
