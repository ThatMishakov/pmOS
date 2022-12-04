#pragma once
#include <types.hh>
#include "utility.hh"

namespace klib {

template<class T, class... Args>
constexpr T* construct_at( T* p, Args&&... args );

template<class T>
class weak_ptr;

template<class T>
class unique_ptr;

template<class T>
class shared_ptr;

template<class T>
class unique_ptr {
private:
    T* ptr;
public:
    typedef T* pointer;
    typedef T element_type;

    constexpr unique_ptr() noexcept: ptr(nullptr) {};
    constexpr unique_ptr(pointer p) noexcept: ptr(p) {};

    unique_ptr(unique_ptr<T>&& p) noexcept:
        ptr(p.ptr)
    {
        p.ptr = nullptr;
    }

    ~unique_ptr()
    {
        if (ptr != nullptr)
            delete ptr;
    }

    unique_ptr& operator=( unique_ptr&& r ) noexcept;
    constexpr pointer release() noexcept
    {
        pointer oldptr = ptr;
        ptr = nullptr;
        return oldptr;
    }
    constexpr void reset( pointer p_ptr = pointer() ) noexcept
    {
        this->~unique_ptr();
        this->ptr = p_ptr;
    }

    void swap( unique_ptr& other ) noexcept
    {
        pointer tmp = this->ptr;
        this->ptr = other->ptr;
        other->ptr = tmp;
    }

    constexpr pointer get() const noexcept
    {
        return this->ptr;
    }

    constexpr explicit operator bool() const noexcept
    {
        return this->ptr != nullptr;
    }

    constexpr T& operator*() const noexcept
    {
        return *ptr;
    }

    constexpr T* operator->() const noexcept
    {
        return get();
    }
};

template< class T, class... Args >
constexpr unique_ptr<T> make_unique( Args&&... args )
{
    return unique_ptr(new T(forward<Args>(args)...));
}

template<class T>
class shared_ptr {
private:
    T* ptr = nullptr;
    struct refcount_str {
        unsigned long shared_refs = 0;
        unsigned long weak_refs = 0;
        Spinlock s;
    };

    refcount_str* refcount = nullptr;
public:
    typedef T element_type;

    constexpr shared_ptr() noexcept: ptr(nullptr), refcount(nullptr) {};
    shared_ptr(const shared_ptr<T>& p): ptr(p.ptr), refcount(p.refcount)
    {
        if (refcount != nullptr) {
            refcount->s.lock();
            refcount->shared_refs += 1;
            refcount->s.unlock();
        }
    }

    shared_ptr(shared_ptr<T>&& p): ptr(p.ptr), refcount(p.refcount)
    {
        p.ptr = nullptr;
        p.refcount = nullptr;
    }

    shared_ptr(unique_ptr<T>&& p)
    {
        unique_ptr<refcount_str> refcount_new = make_unique<refcount_str>();

        ptr = p.release();
        refcount = refcount_new.release();
        refcount->shared_refs = 1; 
    }

    ~shared_ptr()
    {
        if (refcount == nullptr) return;

        refcount->s.lock();
        refcount->shared_refs--;
        if (refcount->shared_refs == 0) {
            delete ptr;

            if (refcount->weak_refs == 0) {
                delete refcount;
                return;
            }
        }
        refcount->s.unlock();

    }

    shared_ptr& operator=(const shared_ptr& r) noexcept
    {
        if (this->ptr == r.ptr) return *this;

        this->~shared_ptr();
        this->ptr = r.ptr;
        this->refcount = r.refcount;

        if (refcount != nullptr) {
            refcount->s.lock();
            refcount->shared_refs += 1;
            refcount->s.unlock();
        }

        return *this;
    }

    shared_ptr& operator=(unique_ptr<T>&& r)
    {
        if (this->ptr == r.ptr) return *this;
        
        this->~shared_ptr();
        
        this->ptr = r.ptr;
        this->refcount = r.refcount;

        r.ptr = nullptr;
        r.refcount = nullptr;

        return *this;
    }

    element_type* get() const noexcept
    {
        return ptr;
    }

    T& operator*() const noexcept
    {
        return *ptr;
    }

    T* operator->() const noexcept
    {
        return get();
    }

    element_type& operator[](unsigned long idx) const
    {
        return get()[idx];
    }

    long use_count() const noexcept
    {
        return refcount ? refcount->shared_refs : 0;
    }

    bool unique() const noexcept
    {
        return use_count() == 1;
    }

    explicit operator bool() const 
    {
        return get() != nullptr;
    }

    friend class weak_ptr<T>;
    
    template< class... Args >
    friend shared_ptr<T> make_shared( Args&&... args );
};

template <class T, class... Args>  shared_ptr<T> make_shared (Args&&... args)
{
    return shared_ptr<T>(make_unique<T>(forward<Args>(args)...));
}

template<class T>
class weak_ptr {
public:
    constexpr weak_ptr() noexcept;
    weak_ptr(const weak_ptr<T>& r) noexcept;
    weak_ptr(const shared_ptr<T>& p) noexcept;
    weak_ptr(weak_ptr<T>&& r) noexcept;
    ~weak_ptr();

    weak_ptr& operator=(const weak_ptr<T>& r) noexcept;
    weak_ptr& operator=(const shared_ptr<T>& r) noexcept;
    weak_ptr& operator=(weak_ptr<T>&& r) noexcept;

    void reset() noexcept;
    void swap(weak_ptr<T>& r) noexcept;

    long use_count() const noexcept;
    bool expired() const noexcept;
    shared_ptr<T> lock() const noexcept;
private:
    T* ptr = nullptr;
    shared_ptr<T>::refcount_str* refcount = nullptr;
};

}