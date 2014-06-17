/**
 * syn-store.hpp
 *
 * A "synonym" store, with support for equivalence classes.
 * Has a special method, "assign", which mimicks an assignemnt
 * assign(a, b) discards any previous value in "a" and puts "a" in the
 * same equivalence class as b. Any change to "b"'s state will be
 * reflected on "a", unless the equivalence class is broken down
 * (through e.g. reset_insert, or another assignment)
 *
 * 2011/10/26:
 * Much of the essence of the syn_store has been abstracted out
 * into primitive_syn_store, so that the "synonym" aspect can be used
 * without mapping states through integers with the 'MakeValueUnique'.
 *
 * \author Andy Chou
 * \date May 29, 2003
 *
 * If you use a "custom" instantiation (see extern templates at the
 * end, and instantiations in store.cpp), you must include
 * syn-store-impl.hpp.
 *
 * (c) 2004-2012 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef AST_SYN_STORE_HPP
#define AST_SYN_STORE_HPP

#include "primitive-syn-store.hpp"
#include "containers/map.hpp"
//#include <boost/bind.hpp>

#include "caching/subset-caching.hpp"
#include "patterns/extend-patterns.hpp"
#include "store.hpp"
#include "store-pattern.hpp"
#include "ast/cc_ast_aux.hpp" // contains_expr_no_fn
#include "ast/cc.ast.hpp"
#include "allocator/vectormap.hpp" // VectorMapA
#include "containers/resizable-bitset.hpp" // resizable_bitset
#include "containers/foreach.hpp" // foreach

// class syn_store_t<T>
//
// A generic mapping from trees to states, with some special functions
// to handle one-level aliasing.  Each tree is mapped to an
// equivalence class (int) by 'equiv_class'.  Then, each equivalence
// class is mapped to a state by 'valueof'.  (In primitive_syn_store_t)
//
// Template arguments are the same as store_t<T>.
//
template<class T,
         class MakeValueUnique,
         class W = null_widen<T>,
    class Cache = subset_cache_t,
    class Copy = ShallowCopy<T> >
class syn_store_t : public subset_state_t, public primitive_syn_store_t<T,Copy> {
public:
    typedef syn_store_t<T, MakeValueUnique, W, Cache, Copy> this_store_t;
    typedef primitive_syn_store_t<T,Copy> primitive_store_t;

    typedef typename primitive_store_t::iterator iterator;
    typedef typename primitive_store_t::const_iterator const_iterator;
    // WTF? Why are these needed for compilation? :
    using primitive_store_t::equiv_class;
    using primitive_store_t::begin;
    using primitive_store_t::find;
    using primitive_store_t::end;
    using primitive_store_t::existing_valueof_ref;
    using primitive_store_t::normalize;

    // This should only be called once per function.
    syn_store_t(mc_arena a, sm_t *sm, W w = W());

    // Copy an existing syn_store_t onto another arena.
    syn_store_t(mc_arena a, const this_store_t &o);
    
    cache_t *create_empty_cache(mc_arena a) const;

    bool has(const Expression *t, const T& val) const;

    void compute_hash(subset_hash_t& v) const;

    void print_pair(ostream &out,
                    const pair<const Expression *, size_t> &p) const;
    
    void print_cache(IndentWriter &ostr, const subset_cache_pair_set_t &s) const;

    bool is_equal(const state_t& oth) const;

    state_t* clone(mc_arena a) const;

    // Set anything in the store with t as a subexpression to the
    // value.
    void set_all(const Expression *t, const T& val);

    // Set anything in the store with t as a subexpression to the
    // value, and also insert t into the store with the value.
    void set_all_insert(const Expression *t, const T& val);

    void reset_all_insert(const Expression *t, const T& val, arena_t *ar);

    string value_at_id_string(size_t id) const;

    void write_as_text(StateWriter& out) const;

    override bool widens() const;

    /**
     * Used if the cache is subset_cache_t.
     **/
    void widen(const subset_cache_pair_set_t &cache);

    struct IfWidened {
        IfWidened(
            const subset_cache_pair_set_t &cache,
            this_store_t *store):
            cache(cache),
            store(store) {
        }
        const subset_cache_pair_set_t &cache;
        this_store_t *store;
    };

    /**
     * Callback for "erase_if" to implement "widen_internal"
     **/
    static bool if_widened_callback(const iterator &i, void *arg);
    bool if_widened(const iterator &i, const subset_cache_pair_set_t &cache);
    
    /**
     * Used if the cache is subset_cache_t and widen is not null_widen
     **/
    void widen_internal(const subset_cache_pair_set_t &cache);

public:
    W value_widener;
    MakeValueUnique * const uniq;
};


// Common synonym store types
typedef syn_store_t<int, int_unique_t> int_syn_store_t;
typedef syn_store_t<const char*, string_unique_t> string_syn_store_t;
typedef syn_store_t<const Expression *, expr_unique_t> ast_syn_store_t;

extern template class syn_store_t<int, int_unique_t>;
extern template class syn_store_t<const char*, string_unique_t>;
extern template class syn_store_t<const Expression*, expr_unique_t>;

#endif
