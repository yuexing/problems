/**
 * checker-syn-store.hpp
 *
 * A smarter syn store for checkers.  It is very similar in concept to
 * the derive_syn_store, except that instead of creating models, you
 * create error messages with events in them.
 *
 * It is invalid to have a mapping to an error but no corresponding
 * mapping to a value, although the converse is allowed.
 *
 * If you use a "custom" instantiation (see extern templates at the
 * end, and instantiations in store.cpp), you must include
 * checker-syn-store-impl.hpp.
 *
 * (c) 2004-2014 Coverity, Inc. All rights reserved worldwide.
 *
 **/
#ifndef AST_CHECKER_SYN_STORE_HPP
#define AST_CHECKER_SYN_STORE_HPP

#include "syn-store.hpp"
#include "errrep/errrep.hpp"
#include "caching/empty-cache.hpp"
#include "compose-nstate.hpp"
#include "../checker.hpp" // checker<->sm casts
#include "libs/exceptions/sup-assert.hpp" // sup_abort

// A store of errors.  Not cached.
typedef syn_store_t<err_t *,
                    invalid_unique_t<err_t *>,
                    null_widen<err_t *>,
                    empty_cache_t,
                    CloneCopy<err_t> > err_store_t;

template<class T,
         class MakeValueUnique,
         class W = null_widen<T>,
         class Cache = subset_cache_t,
         class Copy = ShallowCopy<T> >
class checker_syn_store_t :
             public compose_state<syn_store_t<T, MakeValueUnique, W, Cache, Copy>,
                         err_store_t>
{
public:
    /// A typedef for the data type, to use e.g. in templates
    typedef T data_type;

    // A generic typedef specifying the type of thing -- errors or models --
    // this store maps to.  This allows template code to be generic w.r.t.
    // checker (error) stores vs. deriver (model) stores.
    typedef err_t error_or_model_t;

    typedef syn_store_t<T, MakeValueUnique, W, Cache, Copy> this_store_t;
    typedef checker_syn_store_t<T, MakeValueUnique, W, Cache, Copy>
    this_checker_store_t;
    typedef typename this_store_t::iterator iterator;
    typedef typename this_store_t::const_iterator const_iterator;

    typedef typename err_store_t::iterator error_iterator;
    typedef typename err_store_t::const_iterator const_error_iterator;

    // Constructor.
    checker_syn_store_t(mc_arena a, sm_t *sm);

    err_store_t const *error_store() const;

    err_store_t *error_store();

    this_store_t const *value_store() const;

    this_store_t *value_store();

    // Beginning/end of the value space in the store.
    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();

    bool empty() const;

    const_error_iterator error_begin() const;
    const_error_iterator error_end() const;
    error_iterator error_begin();
    error_iterator error_end();

    bool has(const Expression *t, const T& val) const;

    bool contains(const Expression *t) const;

    //////////////////////////////////////////////////////////////////////
    // Inserting into the store:
    // Structurally equivalent to inserting into the normal store.  All
    // we do is add some edge operations.
    //////////////////////////////////////////////////////////////////////
    void insert(const Expression *t,
                const T& state,
                err_t* error = NULL,
                bool use_tree = false);

    // reset_insert
    //
    // Insert into the store while creating a new equivalence class.
    //
    void reset_insert(const Expression *t, const T& state, err_t *error = NULL);

    size_t equiv_class_count(int equiv_class) const;

    int get_equiv(const Expression *t) const;

    // Returns true if both t1 and t2 are in the same equivalence class or point to the same expression
    bool is_equiv(const Expression *t1,const Expression *t2) const;

    const Expression *get_tree_from_equiv(int eq) const;

    // create_error
    //
    // "create" a model by determining the interface then creating a model.
    //
    err_t* create_error
    (abstract_interp_t &cur_traversal);

    // assign_unknown
    //
    // Mark the tree as having had something unknown assigned to
    // it. We'll forget about previous values we might have known
    // about this expressions or derivatives of it.
    bool assign_unknown(
        const Expression *t,
        vector<const Expression *> *removedExprs = NULL);

    // Handle an assignment.
    // If this is a regular assignment (BIN_ASSIGN) and the source is
    // in store, it will call "assign", and add an event
    // "Assigning: target = src" with the given tag.
    // The caller should make sure that the target is something that
    // it wants in store.
    // See event_list_t::add_assign_event.
    // Suggested tag = "assign"
    bool handle_assign(
        const char *tag,
        const E_assign *assign,
        abstract_interp_t &cur_traversal,
        vector<const Expression *> *removedExprs = NULL);

    // Indicates a variable going out of scope (or being dead)
    bool unscope(
        const E_variable *t,
        vector<const Expression *> *removedExprs = NULL);

    // find_error
    //
    // Find the error for a given tree.
    //
    err_t* find_error(const Expression *t) const;

    void commit_and_erase_error
    (const Expression *t,
     abstract_interp_t &cur_traversal);

    // erase
    //
    // Erase the mapping from tree to state as well as the mapping
    // from tree to model.
    //
    bool erase(const Expression *t,
               vector<const Expression *> *removedExprs = NULL);

    void erase(iterator i);

    bool reset(const Expression *t);

    void reset(iterator i);

    bool erase_all(
        const Expression *t,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_all(
        const Expression *t,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_if(
        bool (*predicate)(const typename this_store_t::iterator &, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_if_expr(
        bool (*predicate)(const Expression *, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    bool erase_if(
        bool (*predicate)(const typename this_store_t::iterator &, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    bool erase_if_expr(
        bool (*predicate)(const Expression *, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    bool erase_pattern(
        AST_patterns::ExpressionPattern &pat,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_pattern(
        AST_patterns::ExpressionPattern &pat,
        vector<const Expression *> *removedExprs = NULL);


    // find
    //
    // Find the value for a given tree.
    //
    const_iterator find(const Expression *t) const;
    iterator find(const Expression *t);

    // assign
    //
    // Simulate assigning t = t2
    //
    bool assign(
        const Expression *t,
        const Expression *t2,
        vector<const Expression *> *removedExprs = NULL
    );

    /**
     * Merge equivalence class of t1 into t2's (similar to an assign)
     * If one of them isn't in store it is added to the other's equiv
     * class.
     **/
    void merge_equiv(const Expression *t1, const Expression *t2);

    // assign_copy_error
    //
    // Assign t1 to t2, BUT copy the error information rather than aliasing the
    // error information as in the other variant of assign.  If you are writing
    // a checker with a binary state (null, not-null), then when you see an
    // assignment between variables, you no longer need to alias error reporting
    // info because the only future possibilties are an error or a kill.
    // Return the new error.
    //
    err_t *assign_copy_error(const Expression *t1, const Expression *t2);

    // clone
    //
    // Important to have this method otherwise we cannot use the dynamic_cast
    // operator because a cloned checker_syn_store_t will have type
    // compose_nstate_t.  static_cast works okay without this method because
    // we are lucky and there is no data in this class right now ...
    //
    state_t* clone(mc_arena a) const;


    // normalize
    //
    // Remove dangling references in both stores.
    //
    void normalize();

    void clear();

protected:

    // Used for dup-ing.  The compose_state copy constructor does a
    // deep copy.
    checker_syn_store_t(mc_arena a, sm_t *sm, const this_checker_store_t &o);
};

typedef checker_syn_store_t<int, int_unique_t> int_checker_syn_store_t;
typedef checker_syn_store_t<const Expression *, expr_unique_t> ast_checker_syn_store_t;

extern template class checker_syn_store_t<int, int_unique_t>;
extern template class checker_syn_store_t<const Expression *, expr_unique_t>;

#endif // ifndef __CHECKER_SYN_STORE__H__
