/**
 * \file derive-syn-store.hpp
 * Combines (somewhat haphazardly) a source_deriver_state_t and a
 * sink_deriver_state_t.
 * This is exclusively meant for existing derivers that haven't been
 * converted to use a more specific state. DO NOT USE IT FOR NEW
 * DERIVERS.
 *
 * If you use a "custom" instantiation (see extern templates at the
 * end, and instantiations in store.cpp), you must include
 * derive-syn-store-impl.hpp.
 *
 * (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef AST_DERIVE_STORE_HPP
#define AST_DERIVE_STORE_HPP

#include "syn-store.hpp"
#include "compose-nstate.hpp"
#include "caching/empty-cache.hpp"
#include "sm/derive-sm.hpp"
#include "set-state.hpp"                // set_state_t
#include "source-deriver-state.hpp"     // model_store_t
#include "sink-deriver-state.hpp"       // ignored_ifaces_set_t

#include "analysis/traversal/abstract-interp.hpp" // g_evil_global_cur_abstract_interp
#include "analysis/ast/patterns/expression-patterns.hpp" // ExpressionPattern


template<class T,
         class MakeValueUnique,
         class W = null_widen<T>,
         class Cache = subset_cache_t,
         class Copy = ShallowCopy<T> >
class legacy_derive_syn_store_t :
    public compose_state3<syn_store_t<T, MakeValueUnique, W, Cache, Copy>,
    model_store_t, ignored_ifaces_set_t>
{
public:
    /// A typedef for the data type, to use e.g. in templates
    typedef T data_type;

    // A generic typedef specifying the type of thing -- errors or models --
    // this store maps to.  This allows template code to be generic w.r.t.
    // checker (error) stores vs. deriver (model) stores.
    typedef model_t error_or_model_t;

    typedef syn_store_t<T, MakeValueUnique, W, Cache, Copy> this_store_t;
    typedef typename this_store_t::iterator iterator;
    typedef typename this_store_t::const_iterator const_iterator;
    typedef legacy_derive_syn_store_t<T, MakeValueUnique, W, Cache>
    this_legacy_derive_syn_store_t;

    typedef typename model_store_t::iterator model_iterator;
    typedef typename model_store_t::const_iterator const_model_iterator;

    // Constructor.
    legacy_derive_syn_store_t(mc_arena a, sm_t *sm);

    const this_store_t *value_store() const;
    this_store_t *value_store();

    // iteration on value store
    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();

    bool empty() const;

    // find
    //
    // Find the value for a given tree.
    //
    iterator find(const Expression *t);


    const model_store_t *model_store() const;
    model_store_t *model_store();
    model_store_t *get_model_store(); // legacy

    const_model_iterator model_begin() const;
    const_model_iterator model_end() const;
    model_iterator model_begin();
    model_iterator model_end();

    ignored_ifaces_set_t::set_type &ignored_ifaces();

    bool has(const Expression *t, const T& val) const;

    bool contains(const Expression *t) const;

    //////////////////////////////////////////////////////////////////////
    // Inserting into the store:
    // Structurally equivalent to inserting into the normal store.  All 
    // we do is add some edge operations.
    //////////////////////////////////////////////////////////////////////
    void insert(const Expression *t, 
                const T& state, 
                model_t* model = NULL,
                bool use_tree = false);

    // create_model
    //
    // "create" a model by determining the interface then creating a model.
    //
    model_t* create_sink_model(const Expression *t,
                                abstract_interp_t &cur_traversal);

    // Indicates that the given interface has been explicitly ignored.
    bool is_ignored(const action_interface_t *iface);

    bool is_ignored(const action_interface_t &iface);

    model_t *if_not_ignored(model_t *m);

    model_t* create_model(const Expression *t,
                                abstract_interp_t &cur_traversal);

    model_t* create_source_model(const Expression *t,
                                 abstract_interp_t &cur_traversal);

    // create_return_model
    //
    // Create a model for a return value.
    //
    model_t* create_return_model(abstract_interp_t &cur_traversal);

    // create_function_model
    //
    // Create a model for the current function as a whole.
    //
    model_t *create_function_model(abstract_interp_t &cur_traversal);

    // Indicates that the given expression is an sink interface that
    // hasn't been ignored.
    bool is_sink_iface(
        const Expression *expr,
        abstract_interp_t &cur_traversal);

    // If the given expression is an sink interface that
    // hasn't been ignored, return that interface.
    const action_interface_t *find_sink_iface(
        const Expression *expr,
        abstract_interp_t &cur_traversal,
        const event_list_t *&alias_events);

    const action_interface_t *find_source_iface(
        const Expression *expr,
        abstract_interp_t &cur_traversal,
        const event_list_t *&alias_events);

    // assign_unknown
    //
    // Mark the tree as having been assigned an unknown value, so the value
    // should not be tracked any longer.  The difference from ignore() is that
    // assign_unknown() will also kill derivative values such as *parm,
    // parm->field().
    //
    void assign_unknown(const Expression *t);

    // ignore
    //
    // Indicate that a given expression, and the interface it
    // represents (if any) should be ignored for the rest of
    // this path.
    //
    void ignore(const Expression *t);

    void unscope(const E_variable *t);

    bool erase_all(const Expression *t);

    bool reset_all(const Expression *t);

    void clear();

    // find_model
    //
    // Find the model for a given tree.
    //
    model_t* find_model(const Expression *t);

    // erase
    //
    // Erase the mapping from tree to state as well as the mapping
    // from tree to model.
    //
    bool erase(const Expression *t);

    void erase(iterator i);

    /**
     * Removes a single tree, without removing its equivalence class.
     **/
    bool reset(const Expression *t);

    bool reset_if(
        bool (*predicate)(const typename this_store_t::iterator &, void *arg),
        void *arg);

    bool reset_if_expr(
        bool (*predicate)(const Expression *, void *arg),
        void *arg);

    bool erase_if(
        bool (*predicate)(const typename this_store_t::iterator &, void *arg),
        void *arg);

    bool erase_if_expr(
        bool (*predicate)(const Expression *, void *arg),
        void *arg);

    bool erase_pattern(AST_patterns::ExpressionPattern &pat);

    bool reset_pattern(AST_patterns::ExpressionPattern &pat);

    // assign
    //
    // Simulate assigning t1 = t2
    //
    void assign(const Expression *t, const Expression *t2);

    /**
     * Merge equivalence class of t1 into t2's (similar to an assign)
     * If one of them isn't in store it is added to the other's equiv
     * class.
     **/
    void merge_equiv(const Expression *t1, const Expression *t2);

    // clone
    state_t* clone(mc_arena a) const;

    // normalize
    // 
    // Remove dangling references in the state store and the model store.
    //
    void normalize();

    size_t equiv_class_count(int equiv_class);

protected:

    // Used for dup-ing.  The compose_state copy constructor does a
    // deep copy.
    legacy_derive_syn_store_t(mc_arena a, sm_t *sm, const this_legacy_derive_syn_store_t &o);

    derive_sm_t *get_derive_sm() const;
};

typedef legacy_derive_syn_store_t<int, int_unique_t> int_legacy_derive_syn_store_t;
typedef legacy_derive_syn_store_t<const Expression *, expr_unique_t> ast_legacy_derive_syn_store_t;

extern template class legacy_derive_syn_store_t<int, int_unique_t>;
extern template class legacy_derive_syn_store_t<const Expression *, expr_unique_t>;

#endif // ifndef __DERIVE_STORE__H__
