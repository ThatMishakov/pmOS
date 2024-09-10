#pragma once
#include "intrusive_bst.hh"
#include "pair.hh"
// utility is broken in kernel, so improvise...
#include <pmos/utility/utility.hh>

namespace pmos::containers
{
template<typename T> class set
{
private:
    struct Node {
        T value;
        RBTreeNode<Node> bst_head;

        Node(T &&value) : value(value) {}
    };

    using tree_type = RedBlackTree < Node, &Node::bst_head,
          detail::TreeCmp<Node, T, &Node::value>>;
    tree_type::RBTreeHead tree;

    // Prepare for Alloc template
    Node *alloc(T && value);
    void dealloc(Node *node);

public:
    set() noexcept        = default;
    set(const set &other) = delete;
    set(set &&other) noexcept;
    ~set() noexcept;

    // Returns empty set on allocation failure
    set clone() noexcept;

    using iterator   = typename tree_type::RBTreeIterator;
    using size_type  = size_t;
    using key_type   = T;
    using value_type = T;
    // node_type

    iterator begin() noexcept;
    iterator end() noexcept;

    bool empty() const noexcept;
    size_type size() const noexcept;
    // max_size

    void clear() noexcept;
    // first == nullptr, second == false => allocation failed
    pair<iterator, bool> insert_noexcept(value_type &&value) noexcept;
    iterator erase(iterator pos) noexcept;
    size_type erase(const key_type &key) noexcept;
    iterator erase(iterator first, iterator last) noexcept;

    void swap(set &other) noexcept;
    // extract
    // merge

    size_type count(const key_type &key) const noexcept;
    iterator find(const key_type &key) noexcept;
    bool contains(const key_type &key) const noexcept;
};

template<typename T>
set<T>::~set() noexcept
{
    clear();
}

template<typename T>
set<T>::iterator set<T>::begin() noexcept
{
    return tree.begin();
}

template<typename T>
set<T>::iterator set<T>::end() noexcept
{
    return tree.end();
}

template<typename T>
set<T>::iterator set<T>::find(const key_type &key) noexcept
{
    return tree.find(key);
}

template<typename T>
set<T>::iterator set<T>::erase(set<T>::iterator pos) noexcept
{
    return tree.erase(pos);
}

template<typename T>
void set<T>::clear() noexcept
{
    iterator it;
    while ((it = begin()) != end()) {
        erase(it);
        dealloc(it);
    }
}

template<typename T>
set<T>::Node *set<T>::alloc(T &&value)
{
    // This could be copying, investigate
    return new Node(pmos::utility::move(value));
}

template<typename T>
void set<T>::dealloc(Node *n)
{
    delete n;
}

template<typename T>
pair<typename set<T>::iterator, bool> set<T>::insert_noexcept(set<T>::value_type &&value) noexcept
{
    // TODO: Use this as hint
    auto it = find(value);
    if (it != end()) {
        return {it, false};
    }

    Node *n = alloc(pmos::utility::move(value));
    if (!n) {
        return {end(), false};
    }

    tree.insert(n);
    return {n, true};
}

} // namespace pmos::containers
