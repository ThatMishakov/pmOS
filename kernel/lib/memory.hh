#pragma once
#include <types.hh>
#include "utility.hh"
#include "cstddef.hh"

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
    template<typename U>
    friend class unique_ptr;

private:
    T* ptr;
public:
    typedef T* pointer;
    typedef T element_type;

    constexpr unique_ptr() noexcept: ptr(nullptr) {};
    constexpr unique_ptr( nullptr_t ) noexcept: ptr(nullptr) {};

    constexpr unique_ptr<T>(pointer p) noexcept: ptr(p) {};

    template<class U>
    constexpr unique_ptr<T>(unique_ptr<U>&& p) noexcept:
        ptr(p.ptr)
    {
        p.ptr = nullptr;
    }

    ~unique_ptr()
    {
        if (ptr != nullptr)
            delete ptr;
    }

    template<class U>
    constexpr unique_ptr<T>& operator=( unique_ptr<U>&& r ) noexcept
    {
        ptr = r.ptr;
        r.ptr = nullptr;
        return *this;
    }

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

    constexpr void swap( unique_ptr& other ) noexcept
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

struct _smart_ptr_refcount_str {
    unsigned long shared_refs = 0;
    unsigned long weak_refs = 0;
    Spinlock s;
};

template<class T>
class shared_ptr {
    template<class U>
    friend class shared_ptr;

private:
    T* ptr = nullptr;
    _smart_ptr_refcount_str* refcount = nullptr;
public:
    typedef T element_type;

    constexpr shared_ptr() noexcept: ptr(nullptr), refcount(nullptr) {};
    constexpr shared_ptr(nullptr_t) noexcept: ptr(nullptr), refcount(nullptr) {};

    template<typename Y>
    shared_ptr(const shared_ptr<Y>& p): ptr(p.ptr), refcount(p.refcount)
    {
        if (refcount != nullptr) {
            refcount->s.lock();
            refcount->shared_refs += 1;
            refcount->s.unlock();
        }
    }

    shared_ptr(const shared_ptr& p): ptr(p.ptr), refcount(p.refcount)
    {
        if (refcount != nullptr) {
            refcount->s.lock();
            refcount->shared_refs += 1;
            refcount->s.unlock();
        }
    }

    constexpr shared_ptr(shared_ptr&& p): ptr(p.ptr), refcount(p.refcount)
    {
        p.ptr = nullptr;
        p.refcount = nullptr;
    }

    template<typename U>
    constexpr shared_ptr(shared_ptr<U>&& p): ptr(p.ptr), refcount(p.refcount)
    {
        p.ptr = nullptr;
        p.refcount = nullptr;
    }

    shared_ptr(unique_ptr<T>&& p)
    {
        unique_ptr<_smart_ptr_refcount_str> refcount_new = make_unique<_smart_ptr_refcount_str>(_smart_ptr_refcount_str());
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
        if (this == &r) return *this;

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

    template<typename U>
    shared_ptr<T>& operator=(const shared_ptr<U>& r) noexcept
    {
        if (this == &r) return *this;

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

    shared_ptr& operator=(shared_ptr&& r) noexcept
    {
        if (this == &r) return *this;

        this->~shared_ptr();

        this->ptr = r.ptr;
        this->refcount = r.refcount;

        r.ptr = nullptr;
        r.refcount = nullptr;

        return *this;
    }

    template<typename U>
    shared_ptr<T>& operator=(shared_ptr<U>&& r) noexcept
    {
        if (this == &r) return *this;

        this->~shared_ptr();

        this->ptr = r.ptr;
        this->refcount = r.refcount;

        r.ptr = nullptr;
        r.refcount = nullptr;

        return *this;
    }

    template<typename U>
    shared_ptr<T>& operator=(unique_ptr<U>&& r)
    {
        unique_ptr<_smart_ptr_refcount_str> new_refcount = nullptr;

        if (r) {
            new_refcount = make_unique<_smart_ptr_refcount_str>(_smart_ptr_refcount_str());
            new_refcount->shared_refs++;
        }

        this->~shared_ptr();

        this->ptr = r.release();
        this->refcount = new_refcount.release();

        return *this;
    }

    constexpr element_type* get() const noexcept
    {
        return ptr;
    }

    constexpr T& operator*() const noexcept
    {
        return *ptr;
    }

    constexpr T* operator->() const noexcept
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

    constexpr explicit operator bool() const 
    {
        return get() != nullptr;
    }

    friend class weak_ptr<T>;
    
    template< class... Args >
    friend shared_ptr<T> make_shared( Args&&... args );
};

template <class T, class... Args> shared_ptr<T> make_shared (Args&&... args)
{
    return shared_ptr<T>(make_unique<T>(forward<Args>(args)...));
}

template< class T, class U >
auto operator==( const shared_ptr<T>& lhs,
                    const shared_ptr<U>& rhs ) noexcept
{
    return lhs.get() == rhs.get();
}

template< class T, class U >
auto operator!=( const shared_ptr<T>& lhs,
                    const shared_ptr<U>& rhs ) noexcept
{
    return lhs.get() != rhs.get();
}

template< class T, class U >
auto operator<( const shared_ptr<T>& lhs,
                    const shared_ptr<U>& rhs ) noexcept
{
    return lhs.get() < rhs.get();
}

template<class T>
class weak_ptr {
    template<typename U>
    friend class weak_ptr;
    
public:
    constexpr weak_ptr() noexcept = default;
    weak_ptr(const weak_ptr<T>& p) noexcept: ptr(p.ptr), refcount(p.refcount)
    {
        if (refcount != nullptr) {
            refcount->s.lock();
            ++refcount->weak_refs;
            refcount->s.unlock();
        }
    }

    weak_ptr(const shared_ptr<T>& p) noexcept: ptr(p.ptr), refcount(p.refcount)
    {
        if (refcount != nullptr) {
            refcount->s.lock();
            ++refcount->weak_refs;
            refcount->s.unlock();
        }
    }

    constexpr weak_ptr(weak_ptr<T>&& r) noexcept: ptr(r.ptr), refcount(r.refcount)
    {
        r.ptr = nullptr;
        r.refcount = nullptr;
    }

    ~weak_ptr()
    {
        if (refcount == nullptr) return;
        bool delete_refcount = false;

        refcount->s.lock();
        refcount->weak_refs--;
        if (refcount->shared_refs == 0 and refcount->weak_refs == 0) {
            delete_refcount = true;
        }
        refcount->s.unlock();

        if (delete_refcount) delete refcount;
    }

    weak_ptr& operator=(const weak_ptr<T>& r) noexcept;
    weak_ptr& operator=(const shared_ptr<T>& r) noexcept;
    constexpr weak_ptr& operator=(weak_ptr<T>&& r) noexcept
    {
        this->~weak_ptr();

        ptr = r.ptr;
        refcount = r.refcount;

        r.ptr = nullptr;
        r.refcount = nullptr;

        return *this;
    }

    void reset() noexcept;
    void swap(weak_ptr<T>& r) noexcept;

    long use_count() const noexcept
    {
        if (refcount == nullptr) return 0;

        return refcount->shared_refs;
    }

    bool expired() const noexcept
    {
        return use_count() == 0;
    }

    shared_ptr<T> lock() const noexcept
    {
        if (refcount == nullptr) return shared_ptr<T>();
        shared_ptr<T> p = shared_ptr<T>();

        refcount->s.lock();
        if (refcount->shared_refs) {
            refcount->shared_refs++;
            p.ptr = ptr;
        }
        refcount->s.unlock();

        if (p.ptr != nullptr) p.refcount = refcount;
        return p;
    }
private:
    T* ptr = nullptr;
    _smart_ptr_refcount_str* refcount = nullptr;
};

}