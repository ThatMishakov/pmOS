#pragma once
#include <cstdint>
#include <lib/stdexcept.hh>

namespace detail
{
template<typename T, auto... ff> static constexpr bool Cmp(const T *a, const T *b)
{
    return ((a->*ff < b->*ff) || ...);
}

// Should be template<typename T, typename Key, auto T::*f>, but
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83417
template<typename T, typename Key, auto f> static constexpr bool Cmp(const Key &a, const T *b)
{
    return a < b->*f;
}

template<typename T, typename Key, auto f> static constexpr bool Cmp(const T *a, const Key &b)
{
    return a->*f < b;
}

template<typename T, typename Key, auto... f> struct TreeCmp {
    constexpr bool operator()(const T &a, const T &b) const { return Cmp<T, f...>(&a, &b); }
    constexpr bool operator()(const Key &a, const T &b) const { return Cmp<T, Key, f...>(a, &b); }
    constexpr bool operator()(const T &a, const Key &b) const { return Cmp<T, Key, f...>(&a, b); }
};
} // namespace detail

template<typename T> struct RBTreeNode {
    // Bit 0 is the color of the node
    T *parent = nullptr;
    T *left   = nullptr;
    T *right  = nullptr;

    RBTreeNode(): parent(nullptr), left(nullptr), right(nullptr) {}

    bool is_red() const;
    void set_color(bool red);

    T *get_parent();
    void set_parent(T *parent);
};

template<typename T, RBTreeNode<T> T::*head_ptr, class Compare> class RedBlackTree final
{
public:
    struct RBTreeIterator {
        T *node = nullptr;

        RBTreeIterator operator++() noexcept;
        RBTreeIterator operator--() noexcept;

        RBTreeIterator operator++(int) noexcept;
        RBTreeIterator operator--(int) noexcept;

        T &operator*() const noexcept;
        T *operator->() const noexcept;

        bool operator==(const RBTreeIterator &other) const noexcept;
        bool operator!=(const RBTreeIterator &other) const noexcept;

        // I don't know if it's a good idea
        operator T *() const noexcept { return node; }
    };

    class RBTreeHead
    {
        T *root = nullptr;

    public:
        RBTreeIterator begin() noexcept;
        static constexpr RBTreeIterator end() noexcept;
        bool empty() const noexcept;

        RBTreeIterator find(const auto &value);
        RBTreeIterator get_smaller_or_equal(const auto &value);

        RBTreeIterator lower_bound(const auto &value);

        void insert(T *node);
        void erase(T *node);
        void erase(RBTreeIterator it);

        T &at(const auto &value);
        const T &at(const auto &value) const;
    };

    static RBTreeIterator self(T &) noexcept;

    // Methods for the red-black tree
    static T *find(T *root, const T &value);
    static T *find(T *root, const auto &value);

    static void insert(T *&root, T *node);
    static void remove(T *&root, T *node);

private:
    static void fix_insert(T *&root, T *node);
    static void fix_remove(T *&root, T *node, T *parent);
    static void transplant(T *&root, T *u, T *v);

    static void rotate_left(T *&root, T *node);
    static void rotate_right(T *&root, T *node);

    RedBlackTree() = delete;
};

constexpr uintptr_t COLOR_MASK = 1;

template<typename T> bool RBTreeNode<T>::is_red() const
{
    return reinterpret_cast<uintptr_t>(parent) & COLOR_MASK;
}

template<typename T> void RBTreeNode<T>::set_color(bool red)
{
    if (red) {
        parent = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(parent) | COLOR_MASK);
    } else {
        parent = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(parent) & ~COLOR_MASK);
    }
}

template<typename T> T *RBTreeNode<T>::get_parent()
{
    return reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(parent) & ~COLOR_MASK);
}

template<typename T> void RBTreeNode<T>::set_parent(T *p)
{
    parent = reinterpret_cast<T *>((reinterpret_cast<uintptr_t>(parent) & COLOR_MASK) |
                                   reinterpret_cast<uintptr_t>(p));
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
T *RedBlackTree<T, bst_head, Compare>::find(T *root, const T &value)
{
    Compare cmp {};
    T *current = root;
    while (current) {
        if (cmp(value, *current)) {
            current = (current->*bst_head).left;
        } else if (cmp(*current, value)) {
            current = (current->*bst_head).right;
        } else {
            return current;
        }
    }

    return nullptr;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
T *RedBlackTree<T, bst_head, Compare>::find(T *root, const auto &value)
{
    Compare cmp {};
    T *current = root;
    while (current) {
        if (cmp(value, *current)) {
            current = (current->*bst_head).left;
        } else if (cmp(*current, value)) {
            current = (current->*bst_head).right;
        } else {
            return current;
        }
    }

    return nullptr;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::insert(T *&root, T *node)
{
    Compare cmp {};
    T *current = root;
    T *parent  = nullptr;

    (node->*bst_head).left   = nullptr;
    (node->*bst_head).right  = nullptr;
    (node->*bst_head).parent = nullptr;

    while (current) {
        parent = current;
        if (cmp(*node, *current)) {
            current = (current->*bst_head).left;
        } else {
            current = (current->*bst_head).right;
        }
    }

    (node->*bst_head).set_parent(parent);
    (node->*bst_head).set_color(true);
    if (!parent) {
        root = node;
        (node->*bst_head).set_color(false);
    } else if (cmp(*node, *parent)) {
        (parent->*bst_head).left = node;
    } else {
        (parent->*bst_head).right = node;
    }

    fix_insert(root, node);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::fix_insert(T *&root, T *new_node)
{
    while ((new_node->*bst_head).get_parent() and
           ((new_node->*bst_head).get_parent()->*bst_head).is_red()) {
        T *parent      = (new_node->*bst_head).get_parent();
        T *grandparent = (parent->*bst_head).get_parent();
        if (parent == (grandparent->*bst_head).left) {
            T *uncle = (grandparent->*bst_head).right;
            if (uncle and (uncle->*bst_head).is_red()) {
                (parent->*bst_head).set_color(false);
                (uncle->*bst_head).set_color(false);
                (grandparent->*bst_head).set_color(true);
                new_node = grandparent;
            } else {
                if (new_node == (parent->*bst_head).right) {
                    new_node = parent;
                    rotate_left(root, new_node);
                    parent      = (new_node->*bst_head).get_parent();
                    grandparent = (parent->*bst_head).get_parent();
                }
                (parent->*bst_head).set_color(false);
                (grandparent->*bst_head).set_color(true);
                rotate_right(root, grandparent);
            }
        } else {
            T *uncle = (grandparent->*bst_head).left;
            if (uncle && (uncle->*bst_head).is_red()) {
                (parent->*bst_head).set_color(false);
                (uncle->*bst_head).set_color(false);
                (grandparent->*bst_head).set_color(true);
                new_node = grandparent;
            } else {
                if (new_node == (parent->*bst_head).left) {
                    new_node = parent;
                    rotate_right(root, new_node);
                    parent      = (new_node->*bst_head).get_parent();
                    grandparent = (parent->*bst_head).get_parent();
                }
                (parent->*bst_head).set_color(false);
                (grandparent->*bst_head).set_color(true);
                rotate_left(root, grandparent);
            }
        }
    }
    (root->*bst_head).set_color(false);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::transplant(T *&root, T *u, T *v)
{
    if (!(u->*bst_head).get_parent()) {
        root = v;
    } else if (u == ((u->*bst_head).get_parent()->*bst_head).left) {
        ((u->*bst_head).get_parent()->*bst_head).left = v;
    } else {
        ((u->*bst_head).get_parent()->*bst_head).right = v;
    }

    if (v)
        (v->*bst_head).set_parent((u->*bst_head).get_parent());
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::rotate_left(T *&root, T *node)
{
    T *right                = (node->*bst_head).right;
    (node->*bst_head).right = (right->*bst_head).left;
    if ((right->*bst_head).left) {
        ((right->*bst_head).left->*bst_head).set_parent(node);
    }

    (right->*bst_head).set_parent((node->*bst_head).get_parent());
    if (!(node->*bst_head).get_parent()) {
        root = right;
    } else if (node == ((node->*bst_head).get_parent()->*bst_head).left) {
        ((node->*bst_head).get_parent()->*bst_head).left = right;
    } else {
        ((node->*bst_head).get_parent()->*bst_head).right = right;
    }

    (right->*bst_head).left = node;
    (node->*bst_head).set_parent(right);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::rotate_right(T *&root, T *node)
{
    T *left                = (node->*bst_head).left;
    (node->*bst_head).left = (left->*bst_head).right;
    if ((left->*bst_head).right) {
        ((left->*bst_head).right->*bst_head).set_parent(node);
    }

    (left->*bst_head).set_parent((node->*bst_head).get_parent());
    if (!(node->*bst_head).get_parent()) {
        root = left;
    } else if (node == ((node->*bst_head).get_parent()->*bst_head).right) {
        (((node->*bst_head)).get_parent()->*bst_head).right = left;
    } else {
        (((node->*bst_head)).get_parent()->*bst_head).left = left;
    }

    (left->*bst_head).right = node;
    (node->*bst_head).set_parent(left);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::remove(T *&root, T *node)
{
    T *current          = node;
    T *child            = nullptr;
    T *parent           = (current->*bst_head).get_parent();
    bool original_color = (current->*bst_head).is_red();
    if (!(current->*bst_head).left) {
        child = (current->*bst_head).right;
        transplant(root, current, (current->*bst_head).right);
    } else if (!(current->*bst_head).right) {
        child = (current->*bst_head).left;
        transplant(root, current, (current->*bst_head).left);
    } else {
        T *successor = (current->*bst_head).right;
        while ((successor->*bst_head).left) {
            successor = (successor->*bst_head).left;
        }

        original_color = (successor->*bst_head).is_red();
        child          = (successor->*bst_head).right;
        parent         = successor;
        if ((successor->*bst_head).get_parent() == current) {
            if (child) {
                (child->*bst_head).set_parent(successor);
            }
        } else {
            parent = (successor->*bst_head).get_parent();
            transplant(root, successor, (successor->*bst_head).right);
            (successor->*bst_head).right = (current->*bst_head).right;
            ((successor->*bst_head).right->*bst_head).set_parent(successor);
        }

        transplant(root, current, successor);
        (successor->*bst_head).left = (current->*bst_head).left;
        ((successor->*bst_head).left->*bst_head).set_parent(successor);
        (successor->*bst_head).set_color((node->*bst_head).is_red());
    }

    if (!original_color) {
        fix_remove(root, child, parent);
    }
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::fix_remove(T *&root, T *node, T *parent)
{
    while (node != root and (!node or !(node->*bst_head).is_red())) {
        if (node == (parent->*bst_head).left) {
            T *sibling = (parent->*bst_head).right;
            if ((sibling->*bst_head).is_red()) {
                (sibling->*bst_head).set_color(false);
                (parent->*bst_head).set_color(true);
                rotate_left(root, parent);
                sibling = (parent->*bst_head).right;
            }

            if ((!(sibling->*bst_head).left or !((sibling->*bst_head).left->*bst_head).is_red()) and
                (!(sibling->*bst_head).right or
                 !((sibling->*bst_head).right->*bst_head).is_red())) {
                (sibling->*bst_head).set_color(true);
                node   = parent;
                parent = (node->*bst_head).get_parent();
            } else {
                if (!(sibling->*bst_head).right or
                    !((sibling->*bst_head).right->*bst_head).is_red()) {
                    ((sibling->*bst_head).left->*bst_head).set_color(false);
                    (sibling->*bst_head).set_color(true);
                    rotate_right(root, sibling);
                    sibling = (parent->*bst_head).right;
                }

                (sibling->*bst_head).set_color((parent->*bst_head).is_red());
                (parent->*bst_head).set_color(false);
                ((sibling->*bst_head).right->*bst_head).set_color(false);
                rotate_left(root, parent);
                node = root;
            }
        } else {
            T *sibling = (parent->*bst_head).left;
            if ((sibling->*bst_head).is_red()) {
                (sibling->*bst_head).set_color(false);
                (parent->*bst_head).set_color(true);
                rotate_right(root, parent);
                sibling = (parent->*bst_head).left;
            }

            if ((!(sibling->*bst_head).right or
                 !((sibling->*bst_head).right->*bst_head).is_red()) and
                (!(sibling->*bst_head).left or !((sibling->*bst_head).left->*bst_head).is_red())) {
                (sibling->*bst_head).set_color(true);
                node   = parent;
                parent = (node->*bst_head).get_parent();
            } else {
                if (!(sibling->*bst_head).left or
                    !((sibling->*bst_head).left->*bst_head).is_red()) {
                    ((sibling->*bst_head).right->*bst_head).set_color(false);
                    (sibling->*bst_head).set_color(true);
                    rotate_left(root, sibling);
                    sibling = (parent->*bst_head).left;
                }

                (sibling->*bst_head).set_color((parent->*bst_head).is_red());
                (parent->*bst_head).set_color(false);
                ((sibling->*bst_head).left->*bst_head).set_color(false);
                rotate_right(root, parent);
                node = root;
            }
        }
    }

    if (node) {
        (node->*bst_head).set_color(false);
    }
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeHead::begin() noexcept
{
    T *current = root;
    if (!current) {
        return RBTreeIterator {nullptr};
    }

    while ((current->*bst_head).left) {
        current = (current->*bst_head).left;
    }

    return RBTreeIterator {current};
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
constexpr RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeHead::end() noexcept
{
    return RBTreeIterator {nullptr};
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
bool RedBlackTree<T, bst_head, Compare>::RBTreeHead::empty() const noexcept
{
    return root == nullptr;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::RBTreeHead::insert(T *node)
{
    RedBlackTree<T, bst_head, Compare>::insert(root, node);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::RBTreeHead::erase(T *node)
{
    RedBlackTree<T, bst_head, Compare>::remove(root, node);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
void RedBlackTree<T, bst_head, Compare>::RBTreeHead::erase(RBTreeIterator it)
{
    RedBlackTree<T, bst_head, Compare>::remove(root, it.node);
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
T *RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator->() const noexcept
{
    return node;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
T &RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator*() const noexcept
{
    return *node;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
bool RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator==(
    const RBTreeIterator &other) const noexcept
{
    return node == other.node;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
bool RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator!=(
    const RBTreeIterator &other) const noexcept
{
    return node != other.node;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator++() noexcept
{
    if ((node->*bst_head).right) {
        node = (node->*bst_head).right;
        while ((node->*bst_head).left) {
            node = (node->*bst_head).left;
        }
    } else {
        T *parent = (node->*bst_head).get_parent();
        while (parent && node == (parent->*bst_head).right) {
            node   = parent;
            parent = (parent->*bst_head).get_parent();
        }
        node = parent;
    }

    return *this;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator++(int) noexcept
{
    auto it = *this;
    ++*this;
    return it;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeIterator::operator--() noexcept
{
    if ((node->*bst_head).left) {
        node = (node->*bst_head).left;
        while ((node->*bst_head).right) {
            node = (node->*bst_head).right;
        }
    } else {
        T *parent = (node->*bst_head).get_parent();
        while (parent && node == (parent->*bst_head).left) {
            node   = parent;
            parent = (parent->*bst_head).get_parent();
        }
        node = parent;
    }

    return *this;
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeHead::find(const auto &value)
{
    return RBTreeIterator {RedBlackTree<T, bst_head, Compare>::find(root, value)};
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeHead::get_smaller_or_equal(const auto &value)
{
    Compare cmp {};
    T *current = root;
    T *result  = nullptr;

    while (current) {
        if (cmp(*current, value)) {
            result  = current;
            current = (current->*bst_head).right;
        } else if (cmp(value, *current)) {
            current = (current->*bst_head).left;
        } else {
            return RBTreeIterator {current};
        }
    }

    return RBTreeIterator {result};
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
RedBlackTree<T, bst_head, Compare>::RBTreeIterator
    RedBlackTree<T, bst_head, Compare>::RBTreeHead::lower_bound(const auto &value)
{
    Compare cmp {};
    T *current = root;
    T *result  = nullptr;

    while (current) {
        if (cmp(*current, value)) {
            current = (current->*bst_head).right;
        } else if (cmp(value, *current)) {
            result  = current;
            current = (current->*bst_head).left;
        } else {
            return RBTreeIterator {current};
        }
    }

    return RBTreeIterator {result};
}

template<typename T, RBTreeNode<T> T::*bst_head, class Compare>
T &RedBlackTree<T, bst_head, Compare>::RBTreeHead::at(const auto &value)
{
    auto result = find(value);
    if (result == end()) {
        throw std::out_of_range("Value not found in the tree");
    }

    return *result;
}