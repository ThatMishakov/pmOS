#pragma once
#include <stddef.h>

template<typename T>
class Vector {
private:
    static const size_t start_size = 64;

    T* ptr = nullptr;
    size_t a_capacity = 0;
    size_t a_size = 0;
public:
    Vector();
    Vector(size_t);
    Vector(Vector&);
    Vector(size_t, const T&);
    ~Vector();

    size_t size() const;
    T& operator[](size_t n);
    const T& operator[](size_t n) const;
    
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
    ptr = new T[a_capacity];
}

template<typename T>
Vector<T>::~Vector()
{
    delete ptr;
}