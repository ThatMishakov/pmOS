#pragma once
#include <stdint.h>

template<typename T>
class Circ_Array {
private:
    static const size_t default_size = 16;
    T** ptr_array;
    size_t array_size;
    size_t index_start;
    size_t elements;
public:
    constexpr Circ_Array();
    Circ_Array(size_t);
    Circ_Array(const Circ_Array&);
    Circ_Array(size_t, T&);
    ~Circ_Array();

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

    void push_front(const T&);
    void pop_front();

    void clear();

    void reserve();

    inline bool empty() const
    {
        return size() == 0;
    }

};

template<typename T>
size_t Circ_Array<T>::size() const
{
    return elements;
}

template<typename T>
constexpr Circ_Array<T>::Circ_Array(): ptr_array(0), array_size(0), index_start(0), elements(0) {};

template<typename T>
Circ_Array<t>::~Circ_Array()
{
    delete []ptr_array;
}