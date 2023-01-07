#pragma once

#include "mutable/util/macro.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <queue>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
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

    struct Node {
        bool leaf;
    };

    ///> the size of BTree nodes (both `INode` and `Leaf`)
    static constexpr size_type NODE_SIZE_IN_BYTES = NodeSizeInBytes;
    ///> the aignment of BTree nodes (both `INode` and `Leaf`)
    static constexpr size_type NODE_ALIGNMENT_IN_BYTES = NodeAlignmentInBytes;

    private:
    using key_value_type = ref_pair<const key_type, mapped_type>;
    using pointer_type = Node*;



    /** Computes the number of key-value pairs per `Leaf`, considering the specified `NodeSizeInBytes`. */
    static constexpr size_type compute_num_keys_per_leaf()
    {
        /* TODO 1.2.1 */       
        // Size taken by pointer to the previous and next leaf
        size_type metadata_size = sizeof(Leaf*);
        // Size taken by flag indicating is leaf node or not
        metadata_size += sizeof(bool);
        // Size taken by size_type indicating the number of current keys in the leaf
        metadata_size += sizeof(size_type);
        size_type entry_size = sizeof(key_value_type); //sizeof(key_type) + sizeof(mapped_type);
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
        size_type entry_size = sizeof(key_type) + sizeof(INode*); //sizeof(key_type) + sizeof(mapped_type);
        // calculate num_key_value_pairs
        size_type num_entries = (NODE_SIZE_IN_BYTES - metadata_size + sizeof(key_type)) / entry_size;
        return num_entries - 1;
       
       
    };

    public:
    ///> the number of key-value pairs per `Leaf`
    static constexpr size_type NUM_KEYS_PER_LEAF = compute_num_keys_per_leaf();
    ///> the number of keys per `INode`
    static constexpr size_type NUM_KEYS_PER_INODE = compute_num_keys_per_inode();

    /** This class implements leaves of the B+-tree. */
    struct alignas(NODE_ALIGNMENT_IN_BYTES) Leaf : Node
    {
        /* TODO 1.2.2 define fields */
        using keys_type = std::array<key_type, NUM_KEYS_PER_LEAF>;
        using values_type = std::array<mapped_type, NUM_KEYS_PER_LEAF>;
        keys_type keys_;
        values_type values_;
        Leaf* next_;
        size_type n_keys_;

        /* TODO 1.2.3 define methods */
        /*----- C'tors -----*/
        public:
        /* Empty C'tor */
        Leaf() {
            Node::leaf = true;
            next_ = nullptr;
            n_keys_ = 0;
            // std::memset(entries, 0, (NUM_KEYS_PER_LEAF) * sizeof(keys_type));

        }
        /* C'tor */
        Leaf(keys_type keys, values_type values)
        : keys_(std::move(keys))
        , values_(std::move(values))
        { 
            n_keys_ = keys_.size();
            // keys_ = keys;
            // values_ = values;
        }

        /*----- Getters -----*/
        public:
        const keys_type & keys() const { return keys_; }
        values_type & values() { return values_; }
        const values_type & values() const { return values_; }
        key_value_type* get_key_value(int index) {
            return ref_pair(&keys_[index], &values_[index]);
        }
        bool has_next() const { return bool(next_); }
        Leaf & next() { return *next_; }
        const Leaf & next() const { return *next_; }
        
        const size_type & size() const { return n_keys_;}

        bool is_full() {
            return NUM_KEYS_PER_LEAF <= n_keys_;
        }
        bool is_empty() {
            return 0 == n_keys_;
        }

        /*----- Setters -----*/
        public:
        Leaf * next(Leaf* new_next)
        { 
            auto old = next_;
            next_ = new_next;
            return old;
        }

        void add_key_value(key_value_type pair) {
            keys_[n_keys_] = pair.first();
            values_[n_keys_] = pair.second();
            n_keys_++;
        }
    };
    static_assert(sizeof(Leaf) <= NODE_SIZE_IN_BYTES, "Leaf exceeds its size limit");

    /** This class implements inner nodes of the B+-tree. */
    struct alignas(NODE_ALIGNMENT_IN_BYTES) INode : Node
    {
        /* TODO 1.3.2 define fields */
        using keys_type = std::array<key_type, NUM_KEYS_PER_INODE>;
        using pointers_type = std::array<pointer_type, NUM_KEYS_PER_INODE + 1>;
        keys_type keys_;
        pointers_type pointers_;
        size_type n_keys_;

        /* TODO 1.3.3 define methods */
        /*----- C'tors -----*/
        public:
        /* Empty C'tor */
        INode() {
            Node::leaf = false;
            n_keys_ = 0;
            // TODO add memset?
            // std::memset(entries, 0, (NUM_KEYS_PER_LEAF) * sizeof(keys_type));

        }
        // TODO add deconstructor?
        /*----- Getters -----*/
        public:
        const keys_type & keys() const { return keys_; }
        pointers_type & pointers() const { return pointers_; }
        const size_type & size() const { return n_keys_;}
        const key_type key(int index) {
            return keys_[index];
        }
        pointer_type pointer(int index) {
            return pointers_[index];
        }
        bool is_full() {
            return NUM_KEYS_PER_INODE + 1 == n_keys_;
        }
        bool is_empty() {
            return 0 == n_keys_;
        }

        /*----- Setters -----*/
        void add_pointer (pointer_type pointer) {
            if(n_keys_ == 0) {
                key_type const key_ = find_rightmost_key(pointer);
                keys_[n_keys_] = key_;
            }
            else {
                key_type const key_ = find_leftmost_key_(pointer);
                keys_[n_keys_-1] = key_;
            }
            pointers_[n_keys_] = pointer;
            n_keys_++;
        }
        // TODO rewrite find_rightmost and leftmost_
        private:
        const key_type find_rightmost_key (Node *node) {
            if(node->leaf) {
                Leaf *leaf = static_cast<Leaf*>(node);
                return (leaf->keys())[leaf->size()-1];
            }
            else {
                INode *inode = static_cast<INode*>(node);
                return find_rightmost_key(inode->pointer(inode->size()-1));
            }
        }
        const key_type find_leftmost_key_(Node *node) {
            if(node->leaf) {
                Leaf *leaf = static_cast<Leaf*>(node);
                return (leaf->keys())[0];
            }
            else {
                INode *inode = static_cast<INode*>(node);
                return find_leftmost_key_(inode->pointer(0));
            }
        }

       
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
            if (leaf_key_idx_ == leaf_->size()) {
                if (leaf_->has_next()) {
                    leaf_key_idx_ = 0;
                    // TODO review & as it is different from codebase
                    leaf_ = &leaf_->next();
                }
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
    Node* root_;
    size_type size_;
    size_type height_;
    Leaf *first_leaf_, *last_leaf_;
    size_type first_key_idx_, last_key_idx_;



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
        // M_unreachable("not implemented");
        /* TODO 1.4.4 */
        size_type height = 0;
        size_type size = std::distance(begin, end);
        Leaf *first_leaf, *last_leaf;
        size_type const keys_per_leaf = NUM_KEYS_PER_LEAF;
        size_type const keys_per_inode = NUM_KEYS_PER_INODE;
        size_type num_leaves = size / keys_per_leaf + (size % keys_per_leaf != 0);
         
        if(size == 0) {
            Leaf *empty_leaf = new Leaf();
            return BTree(empty_leaf, size, height, empty_leaf, empty_leaf);
        }

        std::vector<Node*> inodes, leaves;

        Leaf * current_leaf = new Leaf();
        first_leaf = current_leaf;
        for (It it = begin; it != end; ++it) {
            if(current_leaf->is_full()) {
                Leaf *new_leaf = new Leaf();
                current_leaf->next(new_leaf);
                leaves.push_back(current_leaf);
                current_leaf = new_leaf;
            }
            key_type const it_key = (*it).first;
            mapped_type it_value = (*it).second;
            current_leaf->add_key_value(ref_pair(it_key, it_value));
        }
        leaves.push_back(current_leaf);
        last_leaf = current_leaf;

        if (num_leaves == 1) {
            return BTree(first_leaf, size, height, first_leaf, first_leaf);
        } else {
            while(leaves.size() > 1) {
                INode *current_inode = new INode();
                height++;
                for (size_type i = 0; i < leaves.size(); i++) {
                    if (current_inode->is_full()) {
                        INode *new_inode = new INode();
                        inodes.push_back(current_inode);
                        current_inode = new_inode;
                    }
                    Node *node = leaves[i];
                    current_inode->add_pointer(node);
                }
                inodes.push_back(current_inode);
                leaves = inodes;
                inodes.clear();
            }
        }
        INode *root = static_cast<INode*>(leaves.front());
        height++;

        return BTree(root, size, height, first_leaf, last_leaf);


        /* Create empty BTree */
        // BTree<Key, Value, NodeSizeInBytes> tree;
      
        // /* Set size of the tree to the number of elements in the vector */
        // tree.size_ = 
        // tree.height_ = 0;
        // size_type num_leaves = tree.size_ / keys_per_leaf + (tree.size_ % keys_per_leaf != 0);
        // size_type num_inodes = (num_leaves + keys_per_leaf - 1) / keys_per_leaf;
        // std::cout << "size_ " << tree.size_ << std::endl;
        // std::cout << "keys_per_leaf " << keys_per_leaf << std::endl;

        // std::cout << "num_leaves " << num_leaves << std::endl;
        // std::cout << "num_inodes " << num_inodes << std::endl;

        // //get all the data in a vector
        // // std::vector<std::ref_pair<key_type, mapped_type>> pairs;
        // // for (auto i = begin; i != end; ++i) {
        // //     pairs.push_back(ref_pair((*i)->first,(*i)->second ));
        // // }  
        // // tree.size_ = pairs.size();
        // if (tree.size_ == 0) {
        //     // return empty tree
        //     tree.root_ = nullptr;
        //     tree.leaf_head_ = nullptr;
        //     return tree;
        // }
        // std::queue<std::unique_ptr<Leaf>> leaves;

        // std::unique_ptr<Leaf> current_leaf = std::make_unique<Leaf>();
        // std::unique_ptr<Leaf> *first_leaf = &current_leaf;
        // std::unique_ptr<Leaf> *last_leaf;
        // It it;
        // size_type i;
        // for (it = begin, i = 0; it != end && i < keys_per_leaf; ++it) {
        //     if(current_leaf->is_full()) {

        //         auto new_leaf = std::make_unique<Leaf>();
        //         current_leaf->next(new_leaf);
        //         leaves.push(current_leaf);
        //         current_leaf = new_leaf;
        //     }
        //     // tree.leaf_head_->insert(std::move(it->first), std::move(it->second));
        //     // current_leaf->insert(std::move(it->first), std::move(it->second));
        //     current_leaf->insert(it->first, it->second);
        //     i++;
        // }
        // leaves.push(current_leaf);
        // last_leaf = &current_leaf;
        // if(num_leaves == 1) {
        //     // TODO fix setting root being Node and not leaf
        //     // tree.root_ = std::move(current_leaf);
        //     tree.leaf_head_ = std::move(current_leaf);
        //     return tree;
        // }

        // return tree;
    }

    private:
    BTree()
    { 
        root_ = nullptr;
        size_ = height_ = 0;
        first_leaf_ = nullptr;
        first_key_idx_ = 0;
    }

    BTree(Node* root, size_type size, size_type height, Leaf *first_leaf, Leaf *last_leaf) {
        root_ = root;
        size_ = size;
        height_ = height;
        first_leaf_ = first_leaf;
        last_leaf_ = last_leaf;
        first_key_idx_ = 0;
        last_key_idx_ = (last_leaf->size() ?  last_leaf->size() - 1 : 0);
    }

    // TODO add deconstructor?

    public:
    ///> returns the size of the tree, i.e. the number of key-value pairs
    size_type size() const { /* TODO 1.4.2 */ return size_; }
    ///> returns the number if inner/non-leaf levels, a.k.a. the height
    size_type height() const { /* TODO 1.4.2 */ return height_; }

    /** Returns an `iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    iterator begin() { /* TODO 1.4.3 */ return iterator(first_leaf_, first_key_idx_); }
    /** Returns the past-the-end `iterator`. */
    iterator end() { /* TODO 1.4.3 */ 
        if (size_ == 0) {
            return iterator(first_leaf_, first_key_idx_);
        } 
        return ++iterator(last_leaf_, last_key_idx_);
    } 
    /** Returns an `const_iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    const_iterator begin() const { /* TODO 1.4.3 */ return const_iterator(first_leaf_, first_key_idx_);}
    /** Returns the past-the-end `iterator`. */
    const_iterator end() const { /* TODO 1.4.3 */
        if (size_ == 0) {
            return const_iterator(first_leaf_, first_key_idx_);
        } 
        return ++const_iterator(last_leaf_, last_key_idx_);
    }
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
