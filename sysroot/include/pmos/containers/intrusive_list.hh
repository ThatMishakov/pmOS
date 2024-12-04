#pragma once
#include <stddef.h>

namespace pmos::containers
{

template<typename T> struct DoubleListHead {
    DoubleListHead<T> *next, *prev;
};

template<typename T, DoubleListHead<T> T:: *Head> class CircularDoubleList
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

        // Potentially a very bad idea
        operator T *() const noexcept { return &**this; }
    };

    // Make init() function explicit to have a trivial constructor
    void init() noexcept { head.next = head.prev = &head; }

    void push_front(T *p) noexcept;
    void push_back(T *p) noexcept;
    static void remove(T *p) noexcept;
    static void remove(iterator it) noexcept;
    bool empty() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;

    T &front() noexcept;
    T &back() noexcept;

    const T &front() const noexcept;
    const T &back() const noexcept;
};

template<typename T, DoubleListHead<T> T:: *Head> class InitializedCircularDoubleList
    : public CircularDoubleList<T, Head>
{
public:
    InitializedCircularDoubleList() noexcept { this->init(); }
    ~InitializedCircularDoubleList() noexcept = default;
};

template<typename T, DoubleListHead<T> T:: *Head>
void CircularDoubleList<T, Head>::push_front(T *p) noexcept
{
    (p->*Head).next = head.next;
    (p->*Head).prev = &head;
    head.next->prev = &(p->*Head);
    head.next       = &(p->*Head);
}

template<typename T, DoubleListHead<T> T:: *Head>
void CircularDoubleList<T, Head>::push_back(T *p) noexcept
{
    (p->*Head).prev = head.prev;
    (p->*Head).next = &head;
    head.prev->next = &(p->*Head);
    head.prev       = &(p->*Head);
}

template<typename T, DoubleListHead<T> T:: *Head>
void CircularDoubleList<T, Head>::remove(T *p) noexcept
{
    (p->*Head).prev->next = (p->*Head).next;
    (p->*Head).next->prev = (p->*Head).prev;
}

template<typename T, DoubleListHead<T> T:: *Head>
void CircularDoubleList<T, Head>::remove(iterator it) noexcept
{
    remove(&*it);
}

template<typename T, DoubleListHead<T> T:: *Head>
bool CircularDoubleList<T, Head>::empty() const noexcept
{
    return head.next == &head;
}

template<typename T, DoubleListHead<T> T:: *Head>
typename CircularDoubleList<T, Head>::iterator &
    CircularDoubleList<T, Head>::iterator::operator++() noexcept
{
    current = current->next;
    return *this;
}

template<typename T, DoubleListHead<T> T:: *Head>
typename CircularDoubleList<T, Head>::iterator &
    CircularDoubleList<T, Head>::iterator::operator--() noexcept
{
    current = current->prev;
    return *this;
}

template<typename T, DoubleListHead<T> T:: *Head>
typename CircularDoubleList<T, Head>::iterator
    CircularDoubleList<T, Head>::iterator::operator++(int) noexcept
{
    iterator tmp = *this;
    current      = current->next;
    return tmp;
}

template<typename T, DoubleListHead<T> T:: *Head>
typename CircularDoubleList<T, Head>::iterator
    CircularDoubleList<T, Head>::iterator::operator--(int) noexcept
{
    iterator tmp = *this;
    current      = current->prev;
    return tmp;
}

template<typename T, DoubleListHead<T> T:: *Head>
T &CircularDoubleList<T, Head>::iterator::operator*() const noexcept
{
    return *reinterpret_cast<T *>(reinterpret_cast<char *>(current) -
                                  reinterpret_cast<size_t>(&(reinterpret_cast<T *>(0)->*Head)));
}

template<typename T, DoubleListHead<T> T:: *Head>
T *CircularDoubleList<T, Head>::iterator::operator->() const noexcept
{
    return &**this;
}

template<typename T, DoubleListHead<T> T:: *Head>
bool CircularDoubleList<T, Head>::iterator::operator==(
    const CircularDoubleList<T, Head>::iterator &other) const noexcept
{
    return current == other.current;
}

template<typename T, DoubleListHead<T> T:: *Head>
bool CircularDoubleList<T, Head>::iterator::operator!=(
    const CircularDoubleList<T, Head>::iterator &other) const noexcept
{
    return !(*this == other);
}

template<typename T, DoubleListHead<T> T:: *Head>
typename CircularDoubleList<T, Head>::iterator CircularDoubleList<T, Head>::begin() noexcept
{
    return iterator(head.next);
}

template<typename T, DoubleListHead<T> T:: *Head>
typename CircularDoubleList<T, Head>::iterator CircularDoubleList<T, Head>::end() noexcept
{
    return iterator(&head);
}

template<typename T, DoubleListHead<T> T:: *Head> T &CircularDoubleList<T, Head>::front() noexcept
{
    return *begin();
}

} // namespace pmos::containers