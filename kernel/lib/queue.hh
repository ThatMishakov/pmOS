#pragma once
#include "list.hh"
#include "utility.hh"

namespace klib {

template <class T, class Container = list<T> >
class queue {
private:
    Container C;
public:
    inline bool empty() const
    {
        return C.empty();
    }

    inline size_t size() const
    {
        return C.size();
    }

    inline T& front()
    {
        return C.front();
    }

    inline const T& front() const
    {
        return C.front();
    }

    inline T& back()
    {
        return C.back();
    }

    inline const T& back() const
    {
        return C.back();
    }

    inline void push(const T& val)
    {
        C.push_back(val);
    }

    inline void push(T&& val)
    {
        C.push_back(forward<T>(val));
    }

    inline void emplace(T&& val)
    {
        C.emplace_back(forward<T>(val));
    }

    inline void pop()
    {
        C.pop_front();
    }

    void swap (queue& X)
    {
        C.swap(X.C);
    }
};

}