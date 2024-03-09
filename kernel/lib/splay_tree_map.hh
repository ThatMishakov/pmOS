#pragma once
#include "pair.hh"
#include "../memory/malloc.hh"
#include "utility.hh"
#include "../utils.hh"
#include "stdexcept.hh"

namespace klib {

template<typename K, typename T>
class splay_tree_map {
public:
    typedef pair<const K, T> value_type;
    typedef value_type& reference;
    typedef K key_type;
private:
    struct node {
        node* left = nullptr;
        node* right = nullptr;
        node* parent = nullptr;
        value_type key_data;

        node() = default;
        constexpr node(K key, T data): left(nullptr), right(nullptr), key_data({key, data}) {};
        constexpr node(value_type p): left(nullptr), right(nullptr), key_data(klib::forward<value_type>(p)) {};
    };

    size_t elements;
    mutable node* root;

    void splay(node* n) const;
    void rotate_left(node*) const;
    void rotate_right(node*) const;
    void delete_node(node*);

    static bool is_left_child(node*);

    static node *max(node*);
    static node *min(node*);

    static node *next(node*);

    static node* smaller(node*, const key_type&);
    static node* eq_or_smaller(node*, const key_type&);
    static node* eq_or_larger(node*, const key_type&);
    static node* larger(node*, const key_type&);
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

        constexpr reference operator*() const
        {
            return ptr->key_data;
        }

        constexpr value_type* operator->() const
        {
            return &ptr->key_data;
        }

        iterator operator++() noexcept;

        iterator operator++(int) noexcept;
    };

    constexpr splay_tree_map() noexcept:
        elements(0), root(nullptr) {};
    splay_tree_map(const splay_tree_map&);
    splay_tree_map(splay_tree_map&&);
    ~splay_tree_map();

    void clear() noexcept;

    pair<iterator,bool> insert(const value_type&);
    pair<iterator,bool> insert(value_type&&);

    pair<iterator,bool> emplace(value_type&&);

    void erase(const K&);
    void erase(iterator it) noexcept;
    void erase_if_exists(const K&) noexcept;

    constexpr size_t size() const noexcept;
    constexpr bool empty() const noexcept;

    size_t count(const K&) const noexcept;

    T& operator[](const K& k);

    const T& at(const K&) const;
    T& at(const K&);

    K largest() const;

    T get_copy_or_default(const K&) noexcept;

    iterator begin() const noexcept
    {
        if (root != nullptr)
            splay(min(root));

        return root;
    }

    constexpr iterator end() const noexcept
    {
        return nullptr;
    } 

    iterator find (const key_type& k) noexcept;
    iterator lower_bound (const key_type& k) noexcept;
    iterator upper_bound (const key_type& k) noexcept;

    iterator get_smaller_or_equal(const key_type& k) noexcept;
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
pair<typename splay_tree_map<K,T>::iterator,bool> splay_tree_map<K,T>::insert(const typename splay_tree_map<K,T>::value_type& pair)
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
            return {n, false};
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

    return {c, true};
}

template<typename K, typename T>
pair<typename splay_tree_map<K,T>::iterator, bool> splay_tree_map<K,T>::insert(typename splay_tree_map<K,T>::value_type&& pair)
{
    node *n = nullptr;
    node* temp = root;
    while (temp != nullptr) {
        n = temp;
        if (pair.first < temp->key_data.first) {
            temp = temp->left;
        } else if (temp->key_data.first < pair.first) {
            temp = temp->right;
        } else {
            splay(n);
            temp->key_data.second = forward<T>(pair.second);
            return {n, false};
        }
    }

    node* c = new node(forward<value_type>(pair));
    // c->data = forward<T>(pair.second);
    // c->key = pair.first;

    if (n == nullptr) {
        root = c;
    } else if (pair.first < n->key_data.first) {
        n->left = c;
        c->parent = n;
    } else {
        n->right = c;
        c->parent = n;
    }

    splay(c);

    ++elements;

    return {c, true};
}

template<typename K, typename T>
pair<typename splay_tree_map<K,T>::iterator, bool> splay_tree_map<K,T>::emplace(typename splay_tree_map<K,T>::value_type&& pair)
{
    return insert(klib::move(pair));
}

template<class K, class T>
constexpr size_t splay_tree_map<K,T>::size() const noexcept
{
    return elements;
}

template<class K, class T>
constexpr bool splay_tree_map<K,T>::empty() const noexcept
{
    return size() == 0;
}

template<class K, class T>
size_t splay_tree_map<K,T>::count(const K& key) const noexcept
{
    node* n = root;
    while (n != nullptr and n->key_data.first != key) {
        if (n->key_data.first < key) {
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
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::max(splay_tree_map<K,T>::node* p)
{
    while (p->right != nullptr) {
        p = p->right;
    }
    return p;
}

template<class K, class T>
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::min(splay_tree_map<K,T>::node* p)
{
    while (p->left != nullptr) {
        p = p->left;
    }
    return p;
}

template<class K, class T>
typename splay_tree_map<K,T>::node* splay_tree_map<K,T>::next(splay_tree_map<K,T>::node* p)
{
    if (p->right != nullptr)
        return min(p->right);

    while (p != nullptr and p->parent != nullptr and p != p->parent->left)
        p = p->parent;

    if (p == nullptr)
        return p;
    
    return p->parent;
}


template<class K, class T>
void splay_tree_map<K,T>::erase(const K& key)
{
    node* n = root;

    while (n->key_data.first != key) {
        if (n->key_data.first < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    delete_node(n);
}

template<class K, class T>
void splay_tree_map<K,T>::erase(iterator it) noexcept
{
    node *n = it.ptr;

    if (n != nullptr)
        delete_node(it.ptr);
}

template<class K, class T>
void splay_tree_map<K,T>::erase_if_exists(const K& key) noexcept
{
    node* n = root;

    while (n != nullptr and n->key_data.first != key) {
        if (n->key_data.first < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    if (n != nullptr)
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
T splay_tree_map<K,T>::get_copy_or_default(const K& key) noexcept
{
    node* n = root;

    while (n != nullptr and n->key_data.first != key) {
        if (n->key_data.first < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    if (n == nullptr) return T();

    splay(n);

    return n->key_data.second;
}

template<class K, class T>
T& splay_tree_map<K,T>::operator[](const K& key)
{
    node* n = root;

    while (n != nullptr and n->key_data.first != key) {
        if (n->key_data.first < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    if (n == nullptr) 
        return insert({key, T()}).first->second;

    splay(n);

    return n->key_data.second;
}

template<class K, class T>
T& splay_tree_map<K,T>::at(const K& key)
{
    node* n = root;

    while (n != nullptr and n->key_data.first != key) {
        if (n->key_data.first < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    if (n == nullptr) 
        throw std::out_of_range("splay_three_map::at");

    splay(n);

    return n->key_data.second;
}

template<class K, class T>
splay_tree_map<K,T>::iterator splay_tree_map<K,T>::find(const K& key) noexcept
{
    node* n = root;

    while (n != nullptr and n->key_data.first != key) {
        if (n->key_data.first < key) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    splay(n);

    return n;
}

template<class K, class T>
K splay_tree_map<K,T>::largest() const
{
    node* n = root;
    while (n->right != nullptr)
        n = n->right;

    return n->key_data.first;
}

template<class K, class T>
splay_tree_map<K,T>::node* splay_tree_map<K,T>::smaller(node* n, const key_type& k)
{
    node* result = nullptr;
    while (n != nullptr) {
        if (not (n->key_data.first < k)) {
            n = n->left;
        } else {
            result = n;
            n = n->right;
        }
    }

    return result;
}

template<class K, class T>
splay_tree_map<K,T>::node* splay_tree_map<K,T>::eq_or_smaller(node* n, const key_type& k)
{
    node* result = nullptr;
    while (n != nullptr) {
        if (k < n->key_data.first) {
            n = n->left;
        } else {
            result = n;
            n = n->right;
        }
    }

    return result;
}

template<class K, class T>
splay_tree_map<K,T>::node* splay_tree_map<K,T>::eq_or_larger(node* n, const key_type& k)
{
    node* result = nullptr;
    while (n != nullptr) {
        if (not (n->key_data.first < k)) {
            result = n;
            n = n->left;
        } else {
            n = n->right;
        }
    }

    return result;
}

template<class K, class T>
splay_tree_map<K,T>::iterator splay_tree_map<K,T>::lower_bound (const key_type& k) noexcept
{
    node* n = eq_or_larger(root, k);
    
    if (n != nullptr)
        splay(n);
    
    return n;
}

template<class K, class T>
void splay_tree_map<K,T>::clear () noexcept
{
    while (root != nullptr)
        delete_node(root);
}

template<class K, class T>
splay_tree_map<K,T>::iterator splay_tree_map<K,T>::get_smaller_or_equal(const key_type& k) noexcept
{
    node* n = eq_or_smaller(root, k);
    
    if (n != nullptr)
        splay(n);
    
    return n;
}

template<class K, class T>
splay_tree_map<K,T>::iterator splay_tree_map<K,T>::iterator::operator++() noexcept
{
    ptr = next(ptr);

    return *this;
}

template<class K, class T>
splay_tree_map<K,T>::iterator splay_tree_map<K,T>::iterator::operator++(int) noexcept
{
    iterator temp = *this;
    ++*this;
    return temp;
}

}
