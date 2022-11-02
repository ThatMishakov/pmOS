#pragma once
#include <stddef.h>

namespace klib {

template<typename T>
class list {
private:
    struct Node {
        Node* next = nullptr;
        Node* prev = nullptr;
        T data;
    };

    Node* first = nullptr;
    Node* last = nullptr;
    size_t l_size = 0;
public:
    class iterator {
    private:
        Node* ptr;
    public:
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
    };

    constexpr list();
    list(const list&);
    ~list();

    T& front();
    const T& front() const;

    T& back();
    const T& back() const;

    bool empty() const;

    void push_front(const T&);
    void push_back(const T&);

    void pop_front();
    void pop_back();

    size_t size() const;

    iterator begin();
    iterator end();
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
void list<T>::push_back(const T& k)
{
    Node* n = new Node;
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
}

template<typename T>
size_t list<T>::size() const
{
    return l_size;
}

template<typename T>
bool list<T>::empty() const
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
void list<T>::pop_front()
{
    if (this->first->next == nullptr) {
        this->first = nullptr;
        this->last = nullptr;
        delete this->first;
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

}