#pragma once
#include <stddef.h>

template<typename T> struct DoubleListHead {
    DoubleListHead<T> *next, *prev;
};

template<typename T, DoubleListHead<T> T::*Head> class CircularDoubleList
{
public:
    DoubleListHead<T> head = {nullptr, nullptr};

    class iterator
    {
        friend CircularDoubleList;
        DoubleListHead<T> *current;
        iterator(DoubleListHead<T> *p) noexcept: current(p) {}

    public:
        iterator &operator++() noexcept;
        iterator &operator--() noexcept;

        iterator operator++(int) noexcept;
        iterator operator--(int) noexcept;

        T &operator*() const noexcept;
        T *operator->() const noexcept;

        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;
    };

    // Make init() function explicit to have a trivial constructor
    void init() noexcept { head.next = head.prev = &head; }

    void insert(T *p) noexcept;
    static void remove(T *p) noexcept;
    bool empty() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;
};

template<typename T, DoubleListHead<T> T::*Head>
void CircularDoubleList<T, Head>::insert(T *p) noexcept
{
    p->*Head.next         = head.next;
    p->*Head.prev         = &head;
    head.next->*Head.prev = p;
    head.next             = p;
}

template<typename T, DoubleListHead<T> T::*Head>
void CircularDoubleList<T, Head>::remove(T *p) noexcept
{
    p->*Head.prev->*Head.next = p->*Head.next;
    p->*Head.next->*Head.prev = p->*Head.prev;
}

template<typename T, DoubleListHead<T> T::*Head>
bool CircularDoubleList<T, Head>::empty() const noexcept
{
    return head.next == &head;
}

template<typename T, DoubleListHead<T> T::*Head>
typename CircularDoubleList<T, Head>::iterator &
    CircularDoubleList<T, Head>::iterator::operator++() noexcept
{
    current = current->next;
    return *this;
}

template<typename T, DoubleListHead<T> T::*Head>
typename CircularDoubleList<T, Head>::iterator &
    CircularDoubleList<T, Head>::iterator::operator--() noexcept
{
    current = current->prev;
    return *this;
}

template<typename T, DoubleListHead<T> T::*Head>
typename CircularDoubleList<T, Head>::iterator
    CircularDoubleList<T, Head>::iterator::operator++(int) noexcept
{
    iterator tmp = *this;
    current      = current->next;
    return tmp;
}

template<typename T, DoubleListHead<T> T::*Head>
typename CircularDoubleList<T, Head>::iterator
    CircularDoubleList<T, Head>::iterator::operator--(int) noexcept
{
    iterator tmp = *this;
    current      = current->prev;
    return tmp;
}

template<typename T, DoubleListHead<T> T::*Head>
T &CircularDoubleList<T, Head>::iterator::operator*() const noexcept
{
    // Use offset to the Head member to get the address of the object
    // Don't use offsetof() directly, as it doesn't work
    return *reinterpret_cast<T *>(reinterpret_cast<char *>(current) - (reinterpret_cast<char *>(&((T *)nullptr->*Head) - nullptr)));
}

template<typename T, DoubleListHead<T> T::*Head>
T *CircularDoubleList<T, Head>::iterator::operator->() const noexcept
{
    return &**this;
}

template<typename T, DoubleListHead<T> T::*Head>
bool CircularDoubleList<T, Head>::iterator::operator==(
    const CircularDoubleList<T, Head>::iterator &other) const noexcept
{
    return current == other.current;
}

template<typename T, DoubleListHead<T> T::*Head>
bool CircularDoubleList<T, Head>::iterator::operator!=(
    const CircularDoubleList<T, Head>::iterator &other) const noexcept
{
    return !(*this == other);
}

template<typename T, DoubleListHead<T> T::*Head>
typename CircularDoubleList<T, Head>::iterator CircularDoubleList<T, Head>::begin() noexcept
{
    return iterator(head.next);
}

template<typename T, DoubleListHead<T> T::*Head>
typename CircularDoubleList<T, Head>::iterator CircularDoubleList<T, Head>::end() noexcept
{
    return iterator(&head);
}