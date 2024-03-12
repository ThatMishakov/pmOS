#pragma once
#include <stddef.h>
#include "utility.hh"

namespace klib {

template<typename T>
class list {
private:
    struct Node {
        Node* next = nullptr;
        Node* prev = nullptr;
        T data = T();
    };

    Node* first = nullptr;
    Node* last = nullptr;
    size_t l_size = 0;
public:
    class iterator {
    private:
        Node* ptr = nullptr;
    public:
        iterator() = default;
        constexpr iterator(Node* n): ptr(n) {};

        iterator& operator++() {
            ptr = ptr->next;
            return *this;
        }

        T& operator*()
        {
            return ptr->data;
        }

        bool operator==(iterator k)
        {
            return this->ptr == k.ptr;
        }

        friend list<T>::iterator list<T>::erase(list<T>::iterator pos);
    };

    constexpr list();
    list(const list&);
    list(list&&);
    ~list();

    list& operator=(const list&);
    list& operator=(list&&);

    T& front();
    const T& front() const;

    T& back();
    const T& back() const;

    bool empty() const noexcept;

    iterator push_front(const T&);
    iterator push_front(T&&);
    iterator emplace_front(T&&);

    iterator push_back(const T&);
    iterator push_back(T&&);
    iterator emplace_back(T&&);

    void pop_front() noexcept;
    void pop_back() noexcept;

    size_t size() const noexcept;

    iterator begin();
    iterator end();

    iterator erase(iterator pos);

    void swap (list& x);
    void clear() noexcept;
};

template<typename T>
constexpr list<T>::list():
    first(nullptr), last(nullptr), l_size(0) {};


template<typename T>
list<T>::~list()
{
    Node* p = first;
    while (p != nullptr) {
        Node* current = p;
        p = p->next;
        delete current;
    }
}

template<typename T>
list<T>::list(const list<T>& from)
{
    this->l_size = from.l_size;

    if (from.first == nullptr) {
        this->first = nullptr;
        this->last = nullptr;
    } else {
        Node* p = new Node();
        Node* c = from.first;

        this->first = p;
        p->data = c->data;
        c = c->next;

        while (c != nullptr) {
            p->next = new Node();
            p->next->prev = p;
            p->next->data = c->data;

            c = c->next;
            p = p->next;
        }

        this->last = p;
    }
}

template<typename T>
list<T>::list(list<T>&& from)
{
    this->l_size = from.l_size;
    this->first = from.first;
    this->last = from.last;

    from.l_size = 0;
    from.first = nullptr;
    from.last = nullptr;
}

template<typename T>
list<T>& list<T>::operator=(const list<T>& from)
{
    this->~list();

    this->l_size = from.l_size;

    if (from.first == nullptr) {
        this->first = nullptr;
        this->last = nullptr;
    } else {
        Node* p = new Node();
        Node* c = from.first;

        this->first = p;
        p->data = c->data;
        c = c->next;

        while (c != nullptr) {
            p->next = new Node;
            p->next->prev = p;
            p->next->data = c->data;

            c = c->next;
            p = p->next;
        }

        this->last = p;
    }

    return *this;
}

template<typename T>
list<T>& list<T>::operator=(list<T>&& from)
{
    this->~list();

    this->l_size = from.l_size;
    this->first = from.first;
    this->last = from.last;

    from.l_size = 0;
    from.first = nullptr;
    from.last = nullptr;
    return *this;
}

template<typename T>
list<T>::iterator list<T>::push_back(const T& k)
{
    Node* n = new Node();
    n->data = k;

    if (last == nullptr) {
        first = n;
        last = n;
    } else {
        n->next = last->next;
        n->prev = last;
        last->next = n;
        last = n;
    }
    ++l_size;

    return iterator(n);
}

template<typename T>
list<T>::iterator list<T>::push_front(const T& k)
{
    Node* n = new Node();
    n->data = k;

    if (first == nullptr) {
        first = n;
        last = n;
    } else {
        n->next = first;
        n->prev = nullptr;
        first->prev = n;
        first = n;
    }
    ++l_size;

    return iterator(n);
}

template<typename T>
list<T>::iterator list<T>::push_front(T&& k)
{
    Node* n = new Node();
    n->data = forward<T>(k);

    if (first == nullptr) {
        first = n;
        last = n;
    } else {
        n->next = first;
        n->prev = nullptr;
        first->prev = n;
        first = n;
    }
    ++l_size;

    return iterator(n);
}

template<typename T>
list<T>::iterator list<T>::push_back(T&& k)
{
    Node* n = new Node();
    n->data = forward<T>(k);

    if (last == nullptr) {
        first = n;
        last = n;
    } else {
        n->next = last->next;
        n->prev = last;
        last->next = n;
        last = n;
    }
    ++l_size;

    return iterator(n);
}

template<typename T>
list<T>::iterator list<T>::emplace_back(T&& k)
{
    return push_back(forward<T>(k));
}

template<typename T>
list<T>::iterator list<T>::emplace_front(T&& k)
{
    return push_front(forward<T>(k));
}

template<typename T>
size_t list<T>::size() const noexcept
{
    return l_size;
}

template<typename T>
bool list<T>::empty() const noexcept
{
    return l_size == 0;
}

template<typename T>
const T& list<T>::front() const
{
    return first->data;
}

template<typename T>
T& list<T>::front()
{
    return first->data;
}

template<typename T>
void list<T>::pop_front() noexcept
{
    if (this->first->next == nullptr) {
        delete this->first;
        this->first = nullptr;
        this->last = nullptr;
    } else {
        Node* t = this->first;
        this->first = this->first->next;
        this->first->prev = nullptr;
        delete t;
    }
    --l_size;
}

template<typename T>
typename list<T>::iterator list<T>::begin()
{
    return iterator(first);
}

template<typename T>
typename list<T>::iterator list<T>::end()
{
    return iterator(nullptr);
}

template<typename T>
typename list<T>::iterator list<T>::erase(list<T>::iterator pos)
{
    Node *n = pos.ptr;
    Node* next = n->next;
    Node* prev = n->prev;

    if (prev != nullptr) {
        prev->next = next;
    } else {
        first = next;
    }

    if (next != nullptr) {
        next->prev = prev;
    } else {
        last = prev;
    }

    --l_size;
    delete n;

    return iterator(next);
}

template<typename T>
void list<T>::clear() noexcept
{
    *this = list();
}

}