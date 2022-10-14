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
    size_t size = 0;
public:
    List();
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
};

template<typename T>
List<T>::List():
    first(nullptr), last(nullptr), size(0) {};


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