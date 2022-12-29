#pragma once

#include "mutable/util/macro.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
// TODO remove include? exchange fails
#include <utility>
#include <variant>


/** Require that \tparam T is an *orderable* type, i.e. that two instances of \tparam T can be compared less than and
 * equals. */
template<typename T>
concept orderable = requires (T a, T b) {
    { a <  b } -> std::same_as<bool>;
    { a == b } -> std::same_as<bool>;
};

/** Require that \tparam T is sortable, i.e. that it is `orderable`, movable, and swappable. */
template<typename T>
concept sortable = std::movable<T> and std::swappable<T> and orderable<T>;

/** A replacement for `std::pair`, that does not leak/enforce a particular data layout. */
template<typename First, typename Second>
struct ref_pair
{
    private:
    std::reference_wrapper<First> first_;
    std::reference_wrapper<Second> second_;

    public:
    ref_pair(First &first, Second &second) : first_(first), second_(second) { }

    First & first() const { return first_.get(); }
    Second & second() const { return second_.get(); }
};


/** Implements a B+-tree of \tparam Key - \tparam Value pairs.  The exact size of a tree node is given as \tparam
 * NodeSizeInBytes and the exact node alignment is given as \tparam NodeAlignmentInBytes.  The implementation must
 * guarantee that nodes are properly allocated to satisfy the alignment. */
template<
    typename Key,
    std::movable Value,
    std::size_t NodeSizeInBytes,
    std::size_t NodeAlignmentInBytes = NodeSizeInBytes
>
requires sortable<Key> and std::copyable<Key>
struct BTree
{
    using key_type = Key;
    using mapped_type = Value;
    using size_type = std::size_t;

    ///> the size of BTree nodes (both `INode` and `Leaf`)
    static constexpr size_type NODE_SIZE_IN_BYTES = NodeSizeInBytes;
    ///> the aignment of BTree nodes (both `INode` and `Leaf`)
    static constexpr size_type NODE_ALIGNMENT_IN_BYTES = NodeAlignmentInBytes;

    private:
    /** Computes the number of key-value pairs per `Leaf`, considering the specified `NodeSizeInBytes`. */
    static constexpr size_type compute_num_keys_per_leaf()
    {
        /* TODO 1.2.1 */       
        // std::cout << "node size in bytes " << NODE_SIZE_IN_BYTES << std::endl;
        // std::cout << "size key " << sizeof(key_type) << std::endl;
        // std::cout << "align key " << alignof(key_type) << std::endl;
        // Size taken by pointer to the previous and next leaf
        size_type metadata_size = sizeof(Leaf*) * 2;
        // Size taken by flag indicating is leaf node or not
        metadata_size += sizeof(bool);
        // Size taken by size_type indicating the number of current keys in the leaf
        metadata_size += sizeof(size_type);
        // std::cout << "size ref_pair " << sizeof(ref_pair) << std::endl;
        // TODO check which type is bigger, put them all together and then the other type
        size_type offset = 0;
        size_type key_align = alignof(key_type);
        size_type mapped_align = alignof(mapped_type);
        // add key
        offset = offset + ((key_align - (offset % key_align)) % key_align);
        offset += sizeof(key_type);
        // add mapped
        offset = offset + ((mapped_align - (offset % mapped_align)) % mapped_align);
        offset += sizeof(mapped_type);

        // value of offset for next key-value pair
        size_type entry_size = offset + ((key_align - (offset % key_align)) % key_align);
        // calculate num_key_value_pairs
        size_type num_entries = (NODE_SIZE_IN_BYTES - metadata_size) / entry_size;
        return num_entries;

    };

    /** Computes the number of keys per `INode`, considering the specified `NodeSizeInBytes`. */
    static constexpr size_type compute_num_keys_per_inode()
    {
        /* TODO 1.3.1 */
        // N*(pointer + key) + pointer = total
        // Size taken by flag indicating is leaf node or not
        size_type metadata_size = sizeof(bool);
        // Size taken by size_type indicating the number of current keys in the node
        metadata_size += sizeof(size_type);
       
        return 0;
       
    };

    public:
    ///> the number of key-value pairs per `Leaf`
    static constexpr size_type NUM_KEYS_PER_LEAF = compute_num_keys_per_leaf();
    ///> the number of keys per `INode`
    static constexpr size_type NUM_KEYS_PER_INODE = compute_num_keys_per_inode();

    /** This class implements leaves of the B+-tree. */
    struct alignas(NODE_ALIGNMENT_IN_BYTES) Leaf
    {
        /* TODO 1.2.2 define fields */
        using keys_type = std::array<key_type, NUM_KEYS_PER_LEAF>;
        using values_type = std::array<mapped_type, NUM_KEYS_PER_LEAF>;
        keys_type keys_;
        values_type values_;
        std::unique_ptr<Leaf> previous_;
        std::unique_ptr<Leaf> next_;
        size_type n_keys_;
        // bool root_;

        /* TODO 1.2.3 define methods */
        /*----- C'tors -----*/
        public:
        /* Empty C'tor */
        Leaf() {}
        /* C'tor */
        Leaf(keys_type keys, values_type values)
        : keys_(std::move(keys))
        , values_(std::move(values))
        { 
            // keys_ = keys;
            // values_ = values;
        }

        /*----- Getters -----*/
        public:
        const keys_type & keys() const { return keys_; }
        values_type & values() { return values_; }
        const values_type & values() const { return values_; }
        bool has_previous() const { return bool(previous_); }
        Leaf & previous() { return *previous_; }
        const Leaf & previous() const { return *previous_; }
        bool has_next() const { return bool(next_); }
        Leaf & next() { return *next_; }
        const Leaf & next() const { return *next_; }
        
        const size_type & n_keys() const { return n_keys_;}
        // bool is_root() {
        //     return root_;
        // }
        bool is_full() {
            return NUM_KEYS_PER_LEAF <= n_keys();
        }
        bool is_empty() {
            return 0 == n_keys();
        }

        /*----- Setters -----*/
        private:
        std::unique_ptr<Leaf> next(std::unique_ptr<Leaf> new_next)
        { return std::exchange(next_, std::move(new_next)); }

        std::unique_ptr<Leaf> previous(std::unique_ptr<Leaf> new_previous)
        { return std::exchange(previous_, std::move(new_previous)); }
        
        std::tuple<std::unique_ptr<Leaf>, key_type> split() {
            size_type pivot_position = len(keys_) / 2;
            key_type new_leaf_pivot = keys_[pivot_position];
            // create the new leaf
            keys_type new_keys;
            values_type new_values;
            std::copy(keys_.begin() + pivot_position, keys_.end() , new_keys.begin());
            std::copy(values_.begin() + pivot_position, values_.end() , new_values.begin());
            Leaf new_leaf = new Leaf(new_keys, new_values);
            // update the old leaf, the rest of the keys are consider to not exist, although they still have value
            n_keys_ = pivot_position;
            next(new_leaf);
            return std::make_tuple(new_leaf, new_leaf_pivot);
        }
        
        /* Public methods */
        public:
        std::tuple<std::unique_ptr<Leaf>, key_type> insert(key_type key, mapped_type value) {
            if (is_full()) {
                // split
                std::tuple<std::unique_ptr<Leaf>, key_type> new_leaf_tuple = split();
                std::unique_ptr<Leaf> new_leaf = new_leaf_tuple(0);
                key_type new_leaf_pivot = new_leaf_tuple(1);
                if (key < new_leaf_pivot) {
                    // insert in this leaf
                    insert(key, value);
                    return nullptr;
                } else {
                    // insert in the new leaf
                    new_leaf.insert(key, value);
                    return new_leaf_tuple;
                }
            } else {
                // base condition, inserts key in order
                auto it = std::upper_bound(keys_.begin(), keys_.begin() + n_keys_, key);
                keys_.insert(it, key);
                int index = std::distance(keys_.begin(), it);
                auto it_values = values_.begin() + index;
                values_.insert(it_values, value);
                n_keys_++;
                return nullptr;
            }
        }

        void remove(key_type key) {
            // Find the index of the element to delete
            int index = -1;
            for (int i = 0; i < n_keys_; i++) {
                if (keys_[i] == key) {
                index = i;
                break;
                }
            }
            // If the element was found, shift all the elements after it one position to the left
            if (index != -1) {
                for (int i = index; i < n_keys_ - 1; i++) {
                    keys_[i] = keys_[i + 1];
                }
                n_keys_--;
            } else {
                return;
            }
            // TODO after deleting check if you have the minimum size of key-values and if not merge
            // TODO do we need pointers for previous and next leaf? that is acyclic?
            size_type min_keys = NUM_KEYS_PER_LEAF / 2;
            bool merged = false;
            if (n_keys_ <= min_keys) {
                // Merge with left sibling
                if (has_previous()) {
                    Leaf* previous = previous();
                    size_type previous_keys = previous->n_keys();
                    if (previous_keys + n_keys_ <= NUM_KEYS_PER_LEAF) {
                        merged = true;
                        // Append all keys and values from current leaf to previous
                        for (int i = 0; i < n_keys_; i++) {
                            previous->keys_[previous_keys + i] = keys_[i];
                            previous->values_[previous_keys + i] = values_[i];
                        }
                        previous->n_keys_ += n_keys_;
                        
                        // Remove the current leaf node
                        // TODO
                        // deleteLeaf(*this);
                        // return info to parent inner node
                        // deleteKeyFromNode(parent, parent->keys[index]);
                    } 
                }
                // As second option, merge with right sibling
                if (has_next() && !merged)  {
                    Leaf* next = next();
                    size_type next_keys = next->n_keys();
                    if (next_keys + n_keys_ <= NUM_KEYS_PER_LEAF) {
                        merged = true;
                        // Append all keys and values from next leaf to current
                        for (int i = 0; i < next_keys; i++) {
                            keys_[i + n_keys_] = next->keys_[i];
                            values_[i + n_keys_] = next->values_[i];
                        }
                        n_keys_ += next->n_keys_;
                       

                        // Remove the right leaf node
                        // TODO
                        // deleteLeaf(next);
                        // return info to parent inner node
                        // deleteKeyFromNode(parent, parent->keys[index]);
                    } 
                }

            }
        }
        // private:
        // TODO
        // deleteLeaf()
    };
    static_assert(sizeof(Leaf) <= NODE_SIZE_IN_BYTES, "Leaf exceeds its size limit");

    /** This class implements inner nodes of the B+-tree. */
    struct alignas(NODE_ALIGNMENT_IN_BYTES) INode
    {
        /* TODO 1.3.2 define fields */

        /* TODO 1.3.3 define methods */
    };
    static_assert(sizeof(INode) <= NODE_SIZE_IN_BYTES, "INode exceeds its size limit");

    private:
    template<bool IsConst>
    struct the_iterator
    {
        friend struct BTree;

        static constexpr bool is_const = IsConst;
        using value_type = std::conditional_t<is_const, const mapped_type, mapped_type>;

        private:
        using leaf_type = std::conditional_t<is_const, const Leaf, Leaf>;

        /* TODO 1.4.3 define fields */
        private:
        using pointer = leaf_type*;
        pointer leaf_;
        size_type leaf_key_idx_;

        public:
        the_iterator(the_iterator<false> other)
        requires is_const
        {
            /* TODO 1.4.3 */
            leaf_ = other.leaf_;
            leaf_key_idx_ = other.leaf_key_idx_;
        }
        the_iterator(pointer leaf, size_type leaf_key_idx = 0) : leaf_(leaf), leaf_key_idx_(leaf_key_idx) { }

        bool operator==(the_iterator other) const {
            /* TODO 1.4.3 */
            return leaf_ == other.leaf_ && leaf_key_idx_ == other.leaf_key_idx_;
        }
        bool operator!=(the_iterator other) const { return not operator==(other); }

        the_iterator & operator++() {
            /* TODO 1.4.3 */
            leaf_key_idx_++;
            if (leaf_key_idx_ > leaf_->n_keys()) {
                leaf_key_idx_ = 0;
                leaf_ = &leaf_->next(); 
            }
            return *this;
        }

        the_iterator operator++(int) {
            the_iterator copy(this);
            operator++();
            return copy;
        }

        ref_pair<const key_type, value_type> operator*() const {
            /* TODO 1.4.3 */
            key_type const key = leaf_->keys_[leaf_key_idx_];
            value_type value = leaf_->values_[leaf_key_idx_];
            return ref_pair(key, value);
        }
    };

    template<bool IsConst>
    struct the_range
    {
        static constexpr bool is_const = IsConst;
        using iter_t = the_iterator<is_const>;
        private:
        iter_t begin_, end_;

        public:
        the_range(iter_t begin, iter_t end) : begin_(begin), end_(end) { }

        bool empty() const { return begin() == end(); }

        iter_t begin() const { return begin_; }
        iter_t end() const { return end_; }
    };

    public:
    using iterator = the_iterator<false>;
    using const_iterator = the_iterator<true>;

    using range = the_range<false>;
    using const_range = the_range<true>;

    private:
    /* TODO 1.4.1 define fields */
    using INodeOrLeaf = std::variant<INode, Leaf>;
    std::unique_ptr<INodeOrLeaf> root_;
    std::unique_ptr<Leaf> leaf_head_;
    size_type size_;
    size_type height_;

    public:
    /** Bulkloads the data in the range from `begin` (inclusive) to `end` (exclusive) into a fresh `BTree` and returns
     * it. */
    template<typename It>
    static BTree Bulkload(It begin, It end)
    requires requires (It it) {
        key_type(std::move(it->first));
        mapped_type(std::move(it->second));
    }
    {
        // auto keys_per_leaf = compute_num_keys_per_leaf();
        // TODO remove code. just to debug the constant expression
        std::cout << "size " << BTree :: compute_num_keys_per_leaf () << std::endl;

        /* TODO 1.4.4 */
        M_unreachable("not implemented");
    }

    private:
    BTree() { }

    public:
    ///> returns the size of the tree, i.e. the number of key-value pairs
    size_type size() const { /* TODO 1.4.2 */ return size_; }
    ///> returns the number if inner/non-leaf levels, a.k.a. the height
    size_type height() const { /* TODO 1.4.2 */ return height_; }

    /** Returns an `iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    iterator begin() { /* TODO 1.4.3 */ return iterator(leaf_head_.get()); }
    /** Returns the past-the-end `iterator`. */
    iterator end() { /* TODO 1.4.3 */ return iterator(nullptr);   } 
    /** Returns an `const_iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    const_iterator begin() const { /* TODO 1.4.3 */ return const_iterator(leaf_head_.get());}
    /** Returns the past-the-end `iterator`. */
    const_iterator end() const { /* TODO 1.4.3 */ return const_iterator(nullptr); }
    /** Returns an `const_iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    const_iterator cbegin() const { return begin(); }
    /** Returns the past-the-end `iterator`. */
    const_iterator cend() const { return end(); }

    /** Returns a `const_iterator` to the first element with the given \p key, if any, and `end()` otherwise. */
    const_iterator find(const key_type &key) const {
        /* TODO 1.4.5 */
        M_unreachable("not implemented");
    }
    /** Returns an `iterator` to the first element with the given \p key, if any, and `end()` otherwise. */
    iterator find(const key_type &key) {
        /* TODO 1.4.5 */
        M_unreachable("not implemented");
    }

    /** Returns a `const_range` of all elements with key in the interval `[lo, hi)`, i.e. `lo` including and `hi`
     * excluding. */
    const_range find_range(const key_type &lo, const key_type &hi) const {
        /* TODO 1.4.6 */
        M_unreachable("not implemented");
    }
    /** Returns a `range` of all elements with key in the interval `[lo, hi)`, i.e. `lo` including and `hi` excluding.
     * */
    range find_range(const key_type &lo, const key_type &hi) {
        /* TODO 1.4.6 */
        M_unreachable("not implemented");
    }

    /** Returns a `const_range` of all elements with key equals to \p key. */
    const_range equal_range(const key_type &key) const {
        /* TODO 1.4.7 */
        M_unreachable("not implemented");
    }
    /** Returns a `range` of all elements with key equals to \p key. */
    range equal_range(const key_type &key) {
        /* TODO 1.4.7 */
        M_unreachable("not implemented");
    }
};
