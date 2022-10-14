#include "vector.hh"

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