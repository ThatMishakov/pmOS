#pragma once
#include "pair.hh"
#include "../memory/malloc.hh"
#include "utility.hh"

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

    size_t elements = 0;
    node* root = &NIL;
    static node NIL;

    void erase_node(node*);
    void insert_node(node*);
    void rotate_left(node*);
    void rotate_right(node*);
    void transplant_node(node*,node*);
    void erase_fix(node*);
    void insert_fix(node*);
    node* search(const K&);
    
    static node* min(node*);
    static node* next(node*);

    static node* max(node*);
    static node* prev(node*);
public:
    typedef size_t size_type;
    friend class iterator;

    class iterator {
        friend class set;
    private:
        node* ptr = &NIL;
        constexpr iterator(node* n): ptr(n) {};
    public:
        iterator() = default;

        iterator& operator++();
        iterator operator++(int);

        iterator& operator--();
        iterator operator--(int);

        const K& operator*()
        {
            return ptr->key;
        }

        const K* operator->()
        {
            return &ptr->key;
        }

        bool operator==(iterator k)
        {
            return this->ptr == k.ptr;
        }
    };

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

    pair<iterator,bool> insert(const K&);
    pair<iterator,bool> insert(K&&);
    pair<iterator,bool> emplace(K&&);

    iterator  erase (iterator position);
    size_type erase(const K&);

    void swap(set&);
    void clear();

    size_t count(const K&);

    iterator begin() const noexcept;
    iterator end() const noexcept;
};

template<typename K>
set<K>::set(const set& s)
{
    if (&s == this)
        return;

    set new_set;

    for (iterator i = s.begin(); i != s.end(); ++i)
        new_set.insert(*i);

    swap(new_set);
}

template<typename K>
set<K>::set(set&& s)
{
    this->elements = s.elements;
    this->root = s.root;

    s.elements = 0;
    s.root = &NIL;
}

template<typename K>
set<K>& set<K>::operator=(set&& s)
{
    this->~set();

    this->elements = s.elements;
    this->root = s.root;

    s.elements = 0;
    s.root = &NIL;

    return *this;
}

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
typename set<K>::node* set<K>::next(node* n)
{
    if (n->right != &NIL)
        return min(n->right);

    n = n->parent;
    while (n != &NIL and n == n->parent->right)
        n = n->parent;

    return n;
}

template<typename K>
size_t set<K>::count(const K& key)
{
    node* p = search(key);
    return p != &NIL;
}

template<typename K>
typename set<K>::size_type set<K>::erase(const K& key)
{
    node* p = search(key);
    erase_node(p);
    return 1;
}

template<typename K>
typename set<K>::iterator set<K>::erase(iterator it)
{
    node* n = next(it.ptr);
    erase_node(it.ptr);
    return n;
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
    --elements;
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
                    rotate_left(w);
                    w = x->parent->left;
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

template<class K>
pair<typename set<K>::iterator,bool> set<K>::insert(const K& k)
{
    node* n = new node({k, &NIL, &NIL, &NIL, true});
    insert_node(n);

    return {n, true};
}

template<class K>
pair<typename set<K>::iterator,bool> set<K>::insert(K&& k)
{
    node* n = new node({forward<K>(k), &NIL, &NIL, &NIL, true});
    insert_node(n);

    return {n, true};
}

template<class K>
pair<typename set<K>::iterator,bool> set<K>::emplace(K&& k)
{
    node* n = new node({forward<K>(k), &NIL, &NIL, &NIL, true});
    insert_node(n);

    return {n, true};
}

template<class T> class set<T>::node set<T>::NIL = {{}, &NIL, &NIL, &NIL, false};

template<class K>
void set<K>::insert_node(node* n)
{
    node* y = &NIL;
    node* temp = root;

    while (temp != &NIL) {
        y = temp;
        if (n->key < temp->key)
            temp = temp->left;
        else
            temp = temp->right;
    }

    n->parent = y;
    if (y == &NIL)
        root = n;
    else if (n->key < y->key)
        y->left = n;
    else
        y->right = n;

    ++elements;
    insert_fix(n);    
}

template<class K>
void set<K>::insert_fix(node* n)
{
    while (n->parent->red) {
        if (n->parent == n->parent->parent->left) {
            node* y = n->parent->parent->right;

            if (y->red) {
                n->parent->red = false;
                y->red = false;
                n->parent->parent->red = true;
                n = n->parent->parent;
            } else {
                if (n == n->parent->right) {
                    n = n->parent;
                    rotate_left(n);
                }

                n->parent->red = false;
                n->parent->parent->red = true;
                rotate_right(n->parent->parent);
            }
        } else {
            node* y = n->parent->parent->left;

            if (y->red) {
                n->parent->red = false;
                y->red = false;
                n->parent->parent->red = true;
                n = n->parent->parent;
            } else {
                if (n == n->parent->left) {
                    n = n->parent;
                    rotate_right(n);
                }

                n->parent->red = false;
                n->parent->parent->red = true;
                rotate_left(n);
            }
        }
    }
    root->red = false;
}

template<class K>
typename set<K>::iterator set<K>::begin() const noexcept
{
    return min(root);
}

template<class K>
typename set<K>::iterator set<K>::end() const noexcept
{
    return &NIL;
}

template<class K>
void set<K>::swap(set<K>& swap_with)
{
    set tmp = move(*this);
    *this = move(swap_with);
    swap_with = move(tmp);
}

template<class K>
void set<K>::clear() noexcept
{
    iterator it = begin();
    while (it != end())
        it = erase(it);
}

template<class K>
typename set<K>::iterator& set<K>::iterator::operator++()
{
    ptr = next(ptr);
    return *this;
}

template<class K>
typename set<K>::iterator set<K>::iterator::operator++(int)
{
    node* tmp_ptr = ptr;
    ptr = next(ptr);
    return tmp_ptr;
}

}
