#pragma once
#include "intrusive_bst.hh"
#include <utility>

#include <pmos/utility/utility.hh>

namespace pmos::containers
{

namespace detail
{
    template<typename Node, typename K> struct MapTreeCmp {
        static bool operator()(const K &key, const Node &node) noexcept
        {
            return key < node.value.first;
        }
        static bool operator()(const Node &node, const K &key) noexcept
        {
            return node.value.first < key;
        }
        static bool operator()(const Node &lhs, const Node &rhs) noexcept
        {
            return lhs.value.first < rhs.value.first;
        }
    };
} // namespace detail

template<typename K, typename V> class map
{
public:
    using size_type  = size_t;
    using key_type   = K;
    using value_type = std::pair<const K, V>;
    // node_type

private:
    struct Node {
        value_type value;
        RBTreeNode<Node> bst_head;

        Node(const K &&key, V &&value): value(key, value) {}
    };

    using tree_type = RedBlackTree<Node, &Node::bst_head, detail::MapTreeCmp<Node, K>>;
    tree_type::RBTreeHead tree;

    Node *alloc(value_type &&val);
    void dealloc(Node *node);

public:
    map() noexcept        = default;
    map(const map &other) = delete;
    map(map &&other) noexcept;
    ~map() noexcept;

    // Returns empty map on allocation failure
    map clone() noexcept;

    struct iterator: public tree_type::RBTreeIterator {
        value_type &operator*() noexcept { return this->node->value; }
        value_type *operator->() noexcept { return &this->node->value; }

        iterator() = default;
        iterator(tree_type::RBTreeIterator it): tree_type::RBTreeIterator(it) {}
    };

    struct const_iterator: public tree_type::RBTreeConstIterator {
        const value_type &operator*() noexcept { return this->node->value; }
        const value_type *operator->() noexcept { return &this->node->value; }

        const_iterator() = default;
        const_iterator(tree_type::RBTreeConstIterator it): tree_type::RBTreeConstIterator(it) {}
        const_iterator(iterator it): tree_type::RBTreeConstIterator(it) {}
    };

    iterator begin() noexcept;
    iterator end() noexcept;

    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    bool empty() const noexcept;
    size_type size() const noexcept;
    // max_size

    void clear() noexcept;
    // first == nullptr, second == false => allocation failed
    pair<iterator, bool> insert_noexcept(const value_type &val) noexcept;
    pair<iterator, bool> insert_noexcept(value_type &&val) noexcept;
    iterator erase(iterator pos) noexcept;
    size_type erase(const key_type &key) noexcept;

    iterator find(const key_type &key) noexcept;
    const_iterator find(const key_type &key) const noexcept;

    iterator lower_bound(const key_type &key) noexcept;
    iterator upper_bound(const key_type &key) noexcept;

    void swap(map &other) noexcept;
    // extract
    // merge

    size_type count(const key_type &key) const noexcept;
    bool contains(const key_type &key) const noexcept;
};

template<typename K, typename V> typename map<K, V>::Node *map<K, V>::alloc(map<K, V>::value_type &&val)
{
    return new Node(pmos::utility::move(val.first), pmos::utility::move(val.second));
}

template<typename K, typename V> void map<K, V>::dealloc(Node *n) { delete n; }

template<typename K, typename V> size_t map<K, V>::erase(const K &key) noexcept
{
    auto it = find(key);
    if (it == end()) {
        return 0;
    }

    erase(it);
    dealloc(it);
    return 1;
}

template<typename K, typename V>
pair<typename map<K, V>::iterator, bool> map<K, V>::insert_noexcept(value_type &&val) noexcept
{
    auto it = find(val.first);
    if (it != end()) {
        return {it, false};
    }

    Node *n = alloc(pmos::utility::move(val));
    if (!n) {
        return {end(), false};
    }

    return {tree.insert(n), true};
}

template<typename K, typename V>
typename map<K, V>::iterator map<K, V>::erase(iterator pos) noexcept
{
    return tree.erase(pos);
}

template<typename K, typename V> bool map<K, V>::empty() const noexcept { return tree.empty(); }

template<typename K, typename V> typename map<K, V>::iterator map<K, V>::begin() noexcept
{
    return tree.begin();
}

template<typename K, typename V> typename map<K, V>::iterator map<K, V>::end() noexcept
{
    return tree.end();
}

template<typename K, typename V> typename map<K, V>::const_iterator map<K, V>::cend() const noexcept
{
    return tree.cend();
}

template<typename K, typename V> typename map<K, V>::iterator map<K, V>::find(const K &key) noexcept
{
    return tree.find(key);
}

template<typename K, typename V> typename map<K, V>::const_iterator map<K, V>::find(const K &key) const noexcept
{
    return tree.find(key);
}

template<typename K, typename V> map<K, V>::~map() noexcept { clear(); }

template<typename K, typename V> void map<K, V>::clear() noexcept
{
    iterator it;
    while ((it = begin()) != end()) {
        erase(it);
        dealloc(it);
    }
}

template<typename K, typename V> map<K, V>::size_type map<K, V>::size() const noexcept
{
    return tree.size();
}

template<typename K, typename V> map<K, V>::size_type map<K, V>::count(const key_type &key) const noexcept
{
    return find(key) != cend();
}

} // namespace pmos::containers
