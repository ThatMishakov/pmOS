#pragma once
#include "list.hh"

namespace klib {
template <class T, class Container = list<T> >
class stack {
private:
    Container c;
public:
    inline bool empty() const
    {
        return c.empty();
    }

    inline size_t size() const
    {
        return c.size();
    }

    inline T& top()
    {
        return c.back();
    }

    inline const &T top() const
    {
        return c.back();
    }

    inline void push(const T& t)
    {
        c.push_back(t);
    }

    inline void push(T&& t)
    {
        c.push_back(t);
    }

    inline void emplace(T&& t)
    {
        c.emplace_back(t);
    }

    inline void pop()
    {
        c.pop_back();
    }

    inline void swap (stack& x)
    {
        c.swap(x.c);
    }
};
}