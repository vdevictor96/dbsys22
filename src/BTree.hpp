#pragma once

#include "mutable/util/macro.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <utility>
#include <cstring>



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

template<typename First, typename Second>
ref_pair<First, Second> make_ref_pair(First &first, Second &second) {
    return ref_pair<First, Second>(first, second);
}


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
        }
        /* Des'tor */
        ~INode() {
            for(size_type i = 0; i < n_keys_; i++) {
                if(!(pointers_[i]->leaf)) {
                    INode *inode = static_cast<INode*>(pointers_[i]);
                    delete inode;
                }
                else {
                    Leaf *leaf = static_cast<Leaf*>(pointers_[i]);
                    delete leaf;
                }
            }
        }

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
                key_type const key_ = get_last_key_(pointer);
                keys_[n_keys_] = key_;
            }
            else {
                key_type const key_ = get_first_key_(pointer);
                keys_[n_keys_-1] = key_;
            }
            pointers_[n_keys_] = pointer;
            n_keys_++;
        }
        private:
        const key_type get_last_key_ (Node *node) {
            if(node->leaf) {
                Leaf *leaf = static_cast<Leaf*>(node);
                return (leaf->keys())[leaf->size()-1];
            }
            else {
                INode *inode = static_cast<INode*>(node);
                return get_last_key_(inode->pointer(inode->size()-1));
            }
        }
        const key_type get_first_key_(Node *node) {
            if(node->leaf) {
                Leaf *leaf = static_cast<Leaf*>(node);
                return (leaf->keys())[0];
            }
            else {
                INode *inode = static_cast<INode*>(node);
                return get_first_key_(inode->pointer(0));
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
                    leaf_ = &leaf_->next();
                }
            }
            return *this;
        }

        the_iterator operator++(int) {
            the_iterator copy(*this);
            operator++();
            return copy;
        }

        ref_pair<const key_type, value_type> operator*() const {
            /* TODO 1.4.3 */
            
            return ref_pair<const key_type, value_type>(leaf_->keys_[leaf_key_idx_], leaf_->values_[leaf_key_idx_]);
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
        /* TODO 1.4.4 */
        size_type size = std::distance(begin, end);
        Leaf *first_leaf, *last_leaf;
        size_type const keys_per_leaf = NUM_KEYS_PER_LEAF;
        size_type num_leaves = size / keys_per_leaf + (size % keys_per_leaf != 0);
        size_type height = num_leaves > 1 ? 1 : 0;
        std::vector<Node*> inodes, leaves, children;

        Leaf * current_leaf = new Leaf();
        first_leaf = current_leaf;
        for (It it = begin; it != end; ++it) {
            // if current_leaf is full, create the next leaf and point it
            if(current_leaf->is_full()) {
                Leaf *new_leaf = new Leaf();
                current_leaf->next(new_leaf);
                leaves.push_back(current_leaf);
                current_leaf = new_leaf;
            }
            // add ref_pairs to current leave
            current_leaf->add_key_value(make_ref_pair(it->first, const_cast<mapped_type&>(it->second)));
        }
        leaves.push_back(current_leaf);
        last_leaf = current_leaf;

        children = leaves;
        while(children.size() > 1) {
            INode *current_inode = new INode();
            height++;
            for (size_type i = 0; i < children.size(); i++) {
                // if inode is full create new and add them to inodes temporal (next children)
                if (current_inode->is_full()) {
                    INode *new_inode = new INode();
                    inodes.push_back(current_inode);
                    current_inode = new_inode;
                }
                // add children to inode
                Node *node = children[i];
                current_inode->add_pointer(node);
            }
            inodes.push_back(current_inode);
            // update children to the previous layer inodes
            children = inodes;
            inodes.clear();
            }        
        return BTree(children.front(), size, height, first_leaf, last_leaf);
    }

    private:
    BTree(Node* root, size_type size, size_type height, Leaf *first_leaf, Leaf *last_leaf) {
        root_ = root;
        size_ = size;
        height_ = height;
        first_leaf_ = first_leaf;
        last_leaf_ = last_leaf;
        first_key_idx_ = 0;
        last_key_idx_ = (last_leaf->size() ?  last_leaf->size() - 1 : 0);
    }

    /* Des'tor */
    public:
    ~BTree() {
        //recursively free childrensubtrees
        if(!(root_->leaf)) {
            INode *inode = static_cast<INode*>(root_);
            delete inode; 
        }
        else {
            Leaf *leaf = static_cast<Leaf*>(root_);
            delete leaf;
        }
        size_ = height_ = 0;
    }
    public:
    ///> returns the size of the tree, i.e. the number of key-value pairs
    size_type size() const { /* TODO 1.4.2 */ return size_; }
    ///> returns the number if inner/non-leaf levels, a.k.a. the height
    size_type height() const { /* TODO 1.4.2 */ return height_; }

    /** Returns an `iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    iterator begin() { /* TODO 1.4.3 */ return iterator(first_leaf_, first_key_idx_); }
    /** Returns the past-the-end `iterator`. */
    iterator end() { /* TODO 1.4.3 */ 
        return size_ ? ++iterator(last_leaf_, last_key_idx_) : iterator(last_leaf_, last_key_idx_);
    } 
    /** Returns an `const_iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    const_iterator begin() const { /* TODO 1.4.3 */ return const_iterator(first_leaf_, first_key_idx_);}
    /** Returns the past-the-end `iterator`. */
    const_iterator end() const { /* TODO 1.4.3 */
        return size_ ? ++iterator(last_leaf_, last_key_idx_) : iterator(last_leaf_, last_key_idx_);
    }
    /** Returns an `const_iterator` to the smallest key-value pair of the tree, if any, and `end()` otherwise. */
    const_iterator cbegin() const { return begin(); }
    /** Returns the past-the-end `iterator`. */
    const_iterator cend() const { return end(); }

    /** Returns a `const_iterator` to the first element with the given \p key, if any, and `end()` otherwise. */
    const_iterator find(const key_type &key) const {
        /* TODO 1.4.5 */
        if (size_ < 1) return cend();
        std::pair<Leaf*, size_type> result = find_(key, root_);
        if (result.first == nullptr) {
            return cend();
        } else {
            return const_iterator(result.first , result.second);
        }
    }
    /** Returns an `iterator` to the first element with the given \p key, if any, and `end()` otherwise. */
    iterator find(const key_type &key) {
        /* TODO 1.4.5 */
        if (size_ < 1) return end();
        std::pair<Leaf*, size_type> result = find_(key, root_);
        if (result.first == nullptr) {
            return end();
        } else {
            return iterator(result.first , result.second);
        }
    }

    

    /** Returns a `const_range` of all elements with key in the interval `[lo, hi)`, i.e. `lo` including and `hi`
     * excluding. */
    const_range find_range(const key_type &lo, const key_type &hi) const {
        /* TODO 1.4.6 */
        if (size_ < 1) {
            return const_range(iterator(nullptr, nullptr), iterator(nullptr, nullptr));
        }

        the_iterator begin_it = find_lower_key(lo, root_);
        if ((begin_it == end()) || ((*begin_it).first() >= hi)) {
            return const_range(iterator(nullptr, nullptr), iterator(nullptr, nullptr));
        }

        the_iterator end_it = begin_it;
        while (!(end_it == end())) {
            if (((*end_it).first()) >= hi) {
                break;
            }
            end_it++;
        }
        return const_range(begin_it, end_it);
    }
    /** Returns a `range` of all elements with key in the interval `[lo, hi)`, i.e. `lo` including and `hi` excluding.
     * */
    range find_range(const key_type &lo, const key_type &hi) {
        /* TODO 1.4.6 */
        if (size_ < 1) {
            return range(end(), end());
        }

        iterator begin_it = find_lower_key(lo, root_);
        if ((begin_it == end()) || ((*begin_it).first() >= hi)) {
            return range(end(), end());
        }

        iterator end_it = begin_it;
        while (!(end_it == end())) {
            if (((*end_it).first()) >= hi) {
                break;
            }
            end_it++;
        }
        return range(begin_it, end_it);
    }

    /** Returns a `const_range` of all elements with key equals to \p key. */
    const_range equal_range(const key_type &key) const {
        /* TODO 1.4.7 */
        return find_range(key, key+1);
    }
    /** Returns a `range` of all elements with key equals to \p key. */
    range equal_range(const key_type &key) {
        /* TODO 1.4.7 */
        return find_range(key, key+1);
    }

    private:
    std::pair<Leaf*, size_type> find_ (const key_type &key, Node* root) const {
        Node* current_node = root;
        while (!(current_node->leaf)) {
            INode* inode = static_cast<INode*>(current_node);
            size_type index = 0;
            bool found = false;
            auto inode_keys = inode->keys();
            size_type inode_size = inode->size();
            for (index = 0; index < inode_size - 1; index++) {
                if (key < inode_keys[index]) {
                    found = true;
                    break;
                }
            }
            current_node = inode->pointer(found ? index : inode->size() - 1);
        }

        Leaf* leaf = static_cast<Leaf*>(current_node);
        auto leaf_keys = leaf->keys();
        size_type leaf_size = leaf->size();
        for (size_type i = 0; i < leaf_size; i++) {
            if (leaf_keys[i] == key) {
                return std::make_pair(leaf, i);
            }
        }
        return std::make_pair(nullptr, 0);
    }

    iterator find_lower_key(const key_type& key, Node* root) {
        Node* current_node = root;
        while (!(current_node->leaf)) {
            INode* inode = static_cast<INode*>(current_node);
            size_type index = 0;
            bool found = false;
            for (index = 0; index < inode->size() - 1; index++) {
                if (key <= inode->keys()[index]) {
                    found = true;
                    break;
                }
            }
            current_node = inode->pointer(found ? index : inode->size() - 1);
        }
        Leaf* leaf = static_cast<Leaf*>(current_node);
        for (size_type i = 0; i < leaf->size(); i++) {
            if (leaf->keys()[i] >= key) {
                return iterator(leaf, i);
            }
        }
        // try in next leaf
        if (leaf->has_next()) {
            Leaf *next_leaf = &leaf->next();
            for (size_type i = 0; i < next_leaf->size(); i++) {
                if (next_leaf->keys()[i] >= key) {
                    return iterator(next_leaf, i);
                }
            }
        }
        return end();
    }
};
