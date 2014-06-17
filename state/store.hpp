// Copyright (c) 2004-2014 Coverity, Inc. All rights reserved worldwide.
/**
 * \file store.hpp
 *
 * Store is a state that maps AST nodes to som user-defined value.
 *
 * This one is modified to work with "new" AST
 * AST node comparison is done using astnode_memory_compare
 *
 * \author: Andy Chou
 * \date: May 29, 2003
 *
 * If you use a "custom" instantiation (see extern templates at the
 * end, and instantiations in store.cpp), you must include
 * store-impl.hpp.
 **/
#ifndef AST_STORE_HPP
#define AST_STORE_HPP

#include "arena/arena-containers.hpp" // arena_map_t
#include "macros.hpp"

#include "text/sstream.hpp"
#include "text/stream-utils.hpp" // spaces
#include "patterns/patterns.hpp" // pattern_fun
#include "ast/ast-compare.hpp" // astnode_lt_t
#include "ast/cc_ast_aux.hpp" // contains_expr_no_fn

#include "state.hpp"

#include "caching/subset-caching.hpp"
#include "unique.hpp"
#include "null-widen.hpp"

#include "ast/ast-print-ops.hpp" // For write_as_text


// Copy an element of the store.
// Given an element of the original ("from"), return an element
// suitable to put in the copy; the copy is allocated on the given arena.
template<typename T> struct ShallowCopy {
    T operator()(const T &from, arena_t *ar) const {
        return from;
    }
};

template<typename T> struct CloneCopy {
    T *operator()(const T *from, arena_t *ar) const {
        if(from) {
            return from->clone(ar);
        }
        return NULL;
    }
};

// class store_t<T>
//
// A generic store maps trees to states.  Its purpose is to allow an
// easy way for extensions to manipulate states that are really
// mappings from trees (gotten from pattern matching) to arbitrary
// states that the extension can define.  This class similar to a
// map<const Expression *, T>, specifically you can use insert(), find(), erase(),
// begin() and end().  You can also use operator[], but be careful
// since if the key tree isn't there, an element will be inserted for
// you!
//
// Other template arguments:
// MakeValueUnique = a way to turn a value into a size_t and back
// W = widener class with operator() that takes a two stores and
//     modifies the first store to be higher in the lattice.
// Copy = Allows copying values; see ShallowCopy and CloneCopy above
// for examples.
//
template<class T,
         class MakeValueUnique,
         class W = null_widen<T>,
         class Cache = subset_cache_t,
         class Copy = ShallowCopy<T> >
class store_t : public subset_state_t {
public:
    // STL-compatible types
    typedef const Expression *key_type;
    typedef std::pair<const Expression * const, T> value_type;
    typedef size_t size_type;

    typedef store_t<T, MakeValueUnique, W, Cache, Copy> this_store_t;
    typedef arena_map_t<const Expression *, T, astnode_lt_t> store_map;
    typedef typename store_map::iterator iterator;
    typedef typename store_map::const_iterator const_iterator;
    typedef std::vector<T> Tvect;

    // This should only be called once per function.
    store_t(mc_arena a, sm_t *sm);

    // Copy a store onto another arena.
    store_t(mc_arena a, const this_store_t &o);

    cache_t *create_empty_cache(mc_arena a) const;

    // if use_tree is true, and an identical tree (structurally) is in
    // the store, the new tree replaces it.  This is useful to
    // remember the line number where the last use_tree was true.
    void insert(const Expression *t, const T& state, bool use_tree = false);

    pair<iterator, bool> insert(const pair<const Expression *, T> &p);

    size_t erase(const Expression *t);

    void erase(iterator i);
    // Alias reset to erase for compatibility with other "stores"
    size_t reset(const Expression *t);
    void reset(iterator i);
    /**
     * A wrapper for a predicate to match on key (i->first) instead of iterator
     **/
    template<typename fun> struct key_match_t {
        key_match_t(fun f):f(f){}
        bool operator()(const iterator &i) const{
            return f(i->first);
        }
        fun const f;
    };

    /**
     * Helper function for parameter inference to \c key_match_t
     **/
    template<typename fun> static key_match_t<fun> key_match(fun f) {
        return key_match_t<fun>(f);
    }

    /**
     * Erase all iterators matching \p p
     **/
    template<typename pred> void erase_all_of(pred p);

    /**
     * Same as erase_all_of, used for syn_store "compatibility"
     **/
    template<typename pred> void reset_all_of(pred p);

    // Erase all keys containing fields of t.
    void erase_all_fields(const Expression *t);

    // Erase all keys containing t.
    // Equivalent in a functional way
    //            erase_all_of(key_match(boost::bind(contains_tree_no_fn, _1, t)));
    void erase_all(const Expression *t);

    // Same as erase_all_fields, to allow syntax compatibility with syn_store.
    void reset_all_fields(const Expression *t);

    // Same as erase_all, to allow syntax compatibility with syn_store.
    void reset_all(const Expression *t);

    // Consider "expr" reassigned, without any knowledge of what the
    // new value will be.
    void assign_unknown(const Expression *expr);

    // Indicates a variable going out of scope (or being dead)
    void unscope(const E_variable *expr);

    // Erase anything matching a pattern.
    void erase_pattern(AST_patterns::ExpressionPattern &pat);

    // Erase anything matching a pattern.
    void reset_pattern(AST_patterns::ExpressionPattern &pat);

    // Set anything in the store with t as a subexpression to the
    // value.
    void set_all(const Expression *t, const T& val);

    // Set anything in the store with t as a subexpression to the
    // value, and also insert t into the store with the value.
    void set_all_insert(const Expression *t, const T& val, bool use_tree = false);

    // Simulate an assignment of T = T2 by copying the value from T2
    // and inserting it for T.  Anything that contains T as a subtree
    // is also erased.
    void assign(const Expression *t, const Expression *t2);

    iterator find(const Expression *t);

    const_iterator find(const Expression *t) const;

    bool has(const Expression *t, const T& val);

    bool contains(const Expression *t) const;

    void clear();

    void compute_hash(subset_hash_t& v) const;

    void print_pair(ostream &out,
                    const pair<const Expression *, size_t> &p) const;
    
    void print_cache(IndentWriter &ostr, const subset_cache_pair_set_t &s) const;

    bool is_equal(const state_t& oth) const;

    state_t* clone(mc_arena a) const;

    bool empty();
    
    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;

    size_type size() const;

    T& operator[](const Expression *t);

    string value_at_id_string(size_t id) const;


    void write_as_text(StateWriter& out) const;

    override bool widens() const;

    /**
     * Used if the cache is subset_cache_t.
     **/
    void widen(const subset_cache_pair_set_t &cache);
    
    /**
     * Used if the cache is subset_cache_t and widen is not null_widen
     **/
    void widen_internal(const subset_cache_pair_set_t &cache);

    // This is exposed only for debugging and self-check purposes.
    // Production code should go through one of the other interfaces.
    store_map const &get_private_store_map() const;

protected:
    W value_widener;

    store_map store;
public:
    MakeValueUnique * const uniq;
};

// Common store types
typedef store_t<int, int_unique_t> int_store_t;
typedef store_t<const char*, string_unique_t> string_store_t;
typedef store_t<const Expression *, expr_unique_t> ast_store_t;

// get_killed_trees
//
// Fill in "v" with a list of items in the store that would be
// killed by "kill_store" with "t" as a an argument.
//
template<class Store, class Vect>
static void get_killed_trees(Store *store, const Expression *t, Vect& v) {
    typedef typename Store::iterator iterator;
    for(iterator i = store->begin(); i != store->end(); ++i) {
        if(contains_tree_no_fn(i->first, t)) {
            v.push_back(i->first);
        }
    }
}

extern template class store_t<int, int_unique_t>;
extern template class store_t<const char*, string_unique_t>;
extern template class store_t<const Expression *, expr_unique_t>;

#endif  // __STORE_H__
