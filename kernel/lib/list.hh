#pragma once
#include <stddef.h>

template<typename T>
class List {
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
    constexpr List();
    List(const List&);
    ~List();

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
};

template<typename T>
constexpr List<T>::List():
    first(nullptr), last(nullptr), l_size(0) {};


template<typename T>
List<T>::~List()
{
    Node* p = first;
    while (p != nullptr) {
        Node* current = p;
        p = p->next;
        delete current;
    }
}

template<typename T>
void List<T>::push_back(const T& k)
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
size_t List<T>::size() const
{
    return l_size;
}

template<typename T>
bool List<T>::empty() const
{
    return l_size == 0;
}

template<typename T>
const T& List<T>::front() const
{
    return first->data;
}

template<typename T>
T& List<T>::front()
{
    return first->data;
}

template<typename T>
void List<T>::pop_front()
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