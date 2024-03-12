#pragma once
#include "vector.hh"
#include "functional.hh"

namespace klib {

template<class T, class Container = vector<T>, class Compare = less<typename Container::value_type>>
class priority_queue {
private:
    Container C;
    void move_up(size_t i);
    void move_down(size_t i);
    constexpr size_t parent(size_t i) noexcept {return (i-1)/2;}
    constexpr size_t left(size_t i) noexcept {return 2*i + 1;}
    constexpr size_t right(size_t i) noexcept {return 2*i + 2;}
public:
    typedef Container::value_type value_type;
    typedef Container::const_reference const_reference;
    typedef Container::size_type size_type;

    const_reference top() const;

    [[nodiscard]] bool empty() const
    {
        return C.empty();
    }

    size_type size() const
    {
        return C.size();
    }

    void push( const value_type& value );	
    void push( value_type&& value );
    void emplace( value_type&& value );

    void pop();
};

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::push( const value_type& value )
{
    this->C.push_back(value);
    move_up(C.size() - 1);
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::push( value_type&& value )
{
    this->C.push_back(forward<T>(value));
    move_up(C.size() - 1);
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::move_up(size_t i)
{
    while (i > 0)
    {
        size_t p = parent(i);
        if (Compare()(C[i], C[p])) {
            swap(C[i], C[p]);
        }
        i = p;
    }
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::move_down(size_t i)
{
    T tmp = move(C[i]);
    size_t j = left(i);

    while (j < C.size()) {
        if (j+i < C.size() and Compare()(C[j+i], C[j])) ++j;

        if (Compare()(C[j], tmp)) {
            C[i] = move(C[j]);
            i = j;
            j = left(i);
        } else {
            break;
        }
    }
    C[i] = move(tmp);
}

template<class T, class Container, class Compare>
void priority_queue<T, Container, Compare>::pop()
{
    if (C.size() > 1) {
        C[0] = move(C[C.size() - 1]);
    }
    C.pop_back();

    move_down(0);
}

template<class T, class Container, class Compare>
typename priority_queue<T, Container, Compare>::const_reference priority_queue<T, Container, Compare>::top() const
{
    return C[0];
}

}