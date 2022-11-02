#pragma once
#include <stddef.h>
#include "../malloc.hh"

template<typename T>
class Vector {
private:
    static const size_t start_size = 16;

    T* ptr = nullptr;
    size_t a_capacity = 0;
    size_t a_size = 0;

    void expand(size_t to);
public:
    Vector();
    Vector(size_t);
    Vector(size_t, const T&);
    Vector(const Vector&);
    ~Vector();

    Vector<T>& operator=(const Vector<T>& a);
    Vector<T>& operator=(Vector<T>&& from);
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
Vector<T>::Vector()
{
    a_size = 0;
    a_capacity = start_size;
    ptr = (T*)malloc(sizeof(T)*a_capacity);
}

template<typename T>
Vector<T>::~Vector()
{
    for (typeof(a_size) i = 0; i < a_size; ++i) {
        ptr[i].~T();
    }

    free(ptr);
}

template<typename T>
Vector<T>::Vector(size_t t): 
    ptr(new T[t]), a_capacity(t), a_size(t) {};

template<typename T>
Vector<T>::Vector(size_t t, const T& e): 
    ptr(new T[t]), a_capacity(t), a_size(t)
{
    for (typeof(t) i = 0; i < t; ++i) {
        ptr[i] = e;
    }
}

/* template<typename T>
Vector<T>& Vector<T>::operator=(const Vector<T>& from); */

template<typename T>
Vector<T>& Vector<T>::operator=(Vector<T>&& from)
{
    if (this == &from) return *this;

    this->~Vector<T>();

    this->ptr = from.ptr;
    this->a_capacity = from.a_capacity;
    this->a_size = from.a_size;

    from.ptr = nullptr;
    from.a_size = 0;
    from.a_capacity = 0;

    return *this;
}

template<typename T>
T& Vector<T>::operator*()
{
    return *ptr;
}

template<typename T>
const T& Vector<T>::operator*() const
{
    return *ptr;
}