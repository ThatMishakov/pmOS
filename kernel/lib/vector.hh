#pragma once
#include <stddef.h>
#include "../memory/malloc.hh"

namespace klib {

template<typename T>
class vector {
private:
    static const size_t start_size = 16;

    T* ptr = nullptr;
    size_t a_capacity = 0;
    size_t a_size = 0;

    void expand(size_t to);
public:
    vector();
    vector(size_t);
    vector(size_t, const T&);
    vector(const vector&);
    vector(vector<T>&&);
    ~vector();

    vector<T>& operator=(const vector<T>& a);
    vector<T>& operator=(vector<T>&& from);
    T& operator*();
    const T& operator*() const;

    size_t size() const;
    
    T& front();
    const T& front() const;

    T& back();
    const T& back() const;

    T* data();
    const T* data() const;

    void push_back(const T&);
    void pop_back();

    void clear();

    void reserve();

    inline bool empty() const
    {
        return a_size == 0;
    }
};


template<typename T>
vector<T>::vector()
{
    a_size = 0;
    a_capacity = start_size;
    ptr = (T*)malloc(sizeof(T)*a_capacity);
}

template<typename T>
vector<T>::~vector()
{
    for (size_t i = 0; i < a_size; ++i) {
        ptr[i].~T();
    }

    free(ptr);
}

template<typename T>
vector<T>::vector(size_t t): 
    ptr(new T[t]), a_capacity(t), a_size(t) {};

template<typename T>
vector<T>::vector(size_t t, const T& e): 
    ptr(new T[t]), a_capacity(t), a_size(t)
{
    for (size_t i = 0; i < t; ++i) {
        ptr[i] = e;
    }
}

template<typename T>
vector<T>& vector<T>::operator=(const vector<T>& from)
{
    if (this == &from) return *this;

    this->~vector<T>();

    this->a_capacity = from.a_size;
    this->a_size = from.a_size;
    this->ptr = new T[this->a_size];

    for (size_t i = 0; i < this->a_size; ++i) {
        this->ptr[i] = from.ptr[i];
    }

    return *this;
}

template<typename T>
vector<T>& vector<T>::operator=(vector<T>&& from)
{
    if (this == &from) return *this;

    this->~vector<T>();

    this->ptr = from.ptr;
    this->a_capacity = from.a_capacity;
    this->a_size = from.a_size;

    from.ptr = nullptr;
    from.a_size = 0;
    from.a_capacity = 0;

    return *this;
}

template<typename T>
T& vector<T>::operator*()
{
    return *ptr;
}

template<typename T>
const T& vector<T>::operator*() const
{
    return *ptr;
}

template<typename T>
T& vector<T>::front()
{
    return *ptr;
}

template<typename T>
const T& vector<T>::front() const
{
    return *ptr;
}

template<typename T>
inline size_t vector<T>::size() const
{
    return a_size;
}

}