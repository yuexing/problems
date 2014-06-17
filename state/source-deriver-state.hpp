/**
 * \file source-deriver-state.hpp
 *
 * State useful to write "source" derivers. Keeps track of model_t
 * objects in addition to abstract values.
 *
 * More precisely, this is what it does:
 *
 * - Keeps a map from expressions to (equivalence class, model).
 * and one from equivalence class to abstract value. The reason the
 * model is not associated with the equivalence class is that, in
 * general, there are aliasing events that one may want to apply to
 * one expression but not the other.
 * It is invalid to have a mapping to a model but no corresponding
 * mapping to a value, although the converse is allowed.
 *
 * - Keeps a map from interfaces to models. When an assignment to a
 * source interface is seen, the model for the right hand side, if
 * any, is associated to the interface.
 * The reason is to get this to work:
 *
 * p->foo = source();
 * p = 0;
 * commit_models(); // p->foo is no longer an interface, but we still
 * want "source" to be associated with <parm>->foo
 *
 * It also has a function, commit_all_models, which is meant to be run
 * at eop. There are several reasons to wait for eop:
 * One is to avoid the need to "uncommit" a model:
 * p->foo = source();
 * p->foo = not_source(); // Previous model stale. If we had committed
 * it, we'd have to remove it.
 *
 * One is to prevent the caller from mistaking a value
 * passed as input from a value output:
 * q = p->foo;
 * p->foo = source();
 * sink(q);
 *
 * We want the sink event to happen before the source event, because
 * otherwise a checker could think that sink(source()) is happening,
 * which can cause FPs.
 *
 * Another one is to have "source" events after "write" to the same
 * interface. The "write" deriver runs first, but waits until EOP.
 *
 * If you use a "custom" instantiation (see extern templates at the
 * end, and instantiations in store.cpp), you must include
 * source-deriver-state-impl.hpp.
 *
 * (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
**/
#ifndef SOURCE_DERIVER_STATE_HPP
#define SOURCE_DERIVER_STATE_HPP

#include "compose-nstate.hpp"                     // compose_state
#include "syn-store.hpp"                          // syn_store_t
#include "analysis/sm/model.hpp"                  // model_t
#include "analysis/caching/empty-cache.hpp"       // empty_cache_t

// A store of models.
typedef syn_store_t<model_t *,
    invalid_unique_t<model_t *>,
    null_widen<model_t *>,
    empty_cache_t,
    CloneCopy<model_t> > model_store_t;

// It would make more sense to have a mapping from expression to model
// + value instead of a mapping from expr to value and one from expr
// to model, but, for now, keep the old way.
#if 0
template<typename T> struct ModelAndValue {
    ModelAndValue():
        value(),
        model(NULL) {
    }
    ModelAndValue(const T &v, model_t *m):
        value(v),
        model(m) {
    }

    T value;
    model_t *model;
};

template<typename T, typename ValueCopy> struct ModelAndValueCopy {
    ModelAndValue<T> operator()(const ModelAndValue<T> &v,
                                arena_t *ar) {
        return ModelAndValue<T>(
            ValueCopy()(v, ar),
            model->clone(ar));
    }
};
#endif

// Structure containing both a model and an equivalence class.
// We need to keep the equivalence class so that if it is erased, we
// can forget about the model.
struct model_and_equiv_class_t {
    model_and_equiv_class_t():
        m(NULL), equiv_class(-1)  {
    }
    model_and_equiv_class_t(
        model_t *m,
        int equiv_class
        ):
        m(m),
        equiv_class(equiv_class)  {
    }
    model_t *m;
    int equiv_class;
};

typedef arena_map_t<
    const action_interface_t *,
    model_and_equiv_class_t,
    action_interface_t::ptr_lt_t>
iface_to_model_map_t;


template<class T,
         class MakeValueUnique,
         class W = null_widen<T>,
         class Cache = subset_cache_t,
         class Copy = ShallowCopy<T> >
class source_deriver_state_t :
    public compose_state<syn_store_t<T, MakeValueUnique, W, Cache, Copy>,
    model_store_t>
{
public:
    /// A typedef for the data type, to use e.g. in templates
    typedef T data_type;

    typedef syn_store_t<T, MakeValueUnique, W, Cache, Copy> this_store_t;
    typedef source_deriver_state_t<T, MakeValueUnique, W, Cache, Copy> this_derive_syn_store_t;
    typedef compose_state<this_store_t, model_store_t> super;

    typedef typename this_store_t::iterator iterator;
    typedef typename this_store_t::const_iterator const_iterator;

    typedef typename model_store_t::iterator model_iterator;
    typedef typename model_store_t::const_iterator const_model_iterator;

    // Constructor.
    // "allowOutputParameterInterface" indicates whether models may be
    // generated for output parameters. Set this to false if you only
    // want to generate return models.
    source_deriver_state_t(
        mc_arena a,
        sm_t *sm,
        bool allowOutputParameterInterface = true);

    override void write_as_text(StateWriter& out) const;

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

    bool has(const Expression *t, const T& val) const;

    bool contains(const Expression *t) const;

    /**
     * Insert an abstract value, and possibly a model.
     * If necessary, the model will be associated to the corresponding
     * interface.
     * If a model is added, then its equivalence class will be returned.
     **/
    opt_uint insert(
        const Expression *t,
        const T& state,
        model_t* model,
        abstract_interp_t &cur_traversal);

    /**
     * Create a model for the return value.
     **/
    model_t* create_return_model(abstract_interp_t &cur_traversal);

    /**
     * Create a model, and associate it with the given value in the
     * store, and possible the appropriate interface, then returns
     * it. The interface in the model is unspecified.
     **/
    model_t* insert_new_model(
        const Expression *t,
        const T &state,
        abstract_interp_t &cur_traversal);

    /**
     * Create a model for the current function as a whole.
     **/
    model_t *create_function_model(abstract_interp_t &cur_traversal);

    const action_interface_t *find_iface(
        const Expression *expr,
        abstract_interp_t &cur_traversal,
        const event_list_t *&alias_events,
        bool allow_side_effect);

    /**
     * Mark the expression as having been assigned an unknown value.
     **/
    bool assign_unknown(
        const Expression *t,
        abstract_interp_t &cur_traversal,
        bool allow_side_effect = false,
        vector<const Expression *> *removedExprs = NULL);

    /**
     * See checker_syn_store_t::handle_assign
     **/
    bool handle_assign(const char *tag,
                       const E_assign *assign,
                       abstract_interp_t &cur_traversal,
                       vector<const Expression *> *removedExprs = NULL);

    // Consider the interface represented by "e", if any, to have been
    // reassigned.
    // If "allow_side_effect" is true, then do this even if
    // "e" is a side-effect interface and "is_assignable" is set to true by
    // "find_side_effect_source_iface".
    // derive_sm_t::allow_side_effect_source() is not consulted.
    void reassign_iface(
        const Expression *e,
        bool allow_side_effect,
        abstract_interp_t &cur_traversal);

    bool handleNewExpr(const Expression *e, abstract_interp_t &cur_traversal);

    // Handle a "return" statement, using the given callback to add an
    // event if there's a model to commit.
    // (Unfortunately there's no way to do that more conveniently than
    // using a callback).
    // If "add_event" is called, then "ret->expr" is not NULL.
    void handleReturn(const S_return *ret,
                      abstract_interp_t &cur_traversal,
                      void (*add_event)(
                          model_t *model,
                          const S_return *ret,
                          abstract_interp_t &cur_traversal,
                          void *arg),
                      void *arg);

    bool unscope(const E_variable *var,
                 vector<const Expression *> *removedExprs = NULL);

    bool remove_transients(vector<const Expression *> *removedExprs = NULL);

    bool erase_all(
        const Expression *t,
        vector<const Expression *> *removedExprs = NULL);

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
    bool erase(
        const Expression *t,
        vector<const Expression *> *removedExprs = NULL);

    /**
     * Removes a single tree, without removing its equivalence class.
     **/
    bool reset(const Expression *t);

    /**
     * Removes from the store iterators for which the predicate is
     * true.
     **/
    bool reset_if(
        bool (*predicate)(const typename this_store_t::iterator &, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_if_expr(
        bool (*predicate)(const Expression *, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_pattern(
        AST_patterns::ExpressionPattern &pat,
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

    // assign
    //
    // Simulate assigning t = t2
    //
    // If "allow_side_effect" is true, then this will transfer the
    // properties of "t2" to "t" even if an assignment should not do
    // that, namely, if assigning to a value parameter. See
    // "allow_side_effect" in reassign_iface.
    bool assign(
        const Expression *t,
        const Expression *t2,
        abstract_interp_t &cur_traversal,
        bool allow_side_effect = false,
        vector<const Expression *> *removedExprs = NULL);

    // Indicate that the given model applies to the given interface.
    // If "doClone" is true, then the model will be cloned before
    // possible alias events are added to it (this is useful for
    // assignment)
    void assign_model_to_iface(
        const Expression *ifaceExpr,
        model_t *m,
        int equiv_class,
        bool allow_side_effect,
        bool doClone,
        abstract_interp_t &cur_traversal);

    // clone
    state_t* clone(mc_arena a) const;

    // normalize
    //
    // Remove dangling references in the state store and the model store.
    //
    void normalize();

    size_t equiv_class_count(int equiv_class);

    // Calls commit_all_models.
    void on_fn_eop(abstract_interp_t &cur_traversal);

    // Discard all models; prevents "on_fn_eop" from committing any models.
    void discard_all_models();

protected:

    // Used for dup-ing.  The compose_state copy constructor does a
    // deep copy.
    source_deriver_state_t(
        arena_t *a,
        sm_t *sm,
        const this_derive_syn_store_t &o);

    derive_sm_t *get_derive_sm() const;

    void normalize_iface_to_model();

    void commit_all_models(abstract_interp_t &cur_traversal);

    // Data
public:
    // Map from model to equivalence class in model_store.
    // At present, it doesn't participate in caching. The reason is
    // that:
    // - It would be a fair amount of effort
    // - In most practical cases, the value_store will provide
    // sufficient cache misses.
    iface_to_model_map_t iface_to_model;

    // See ctor argument of the same name.
    // If false, "iface_to_model" will always be empty.
    bool const allowOutputParameterInterface;
};

// Callback for handleReturn above.
// Adds:
// Returning <expr>.
// with tag "return".
// It's not a member function of source_deriver_state_t because I
// don't want it to be a template.
// Implemented in sink-deriver-state.cpp.
// The argument should be NULL.
void addSimpleReturnEvent(
    model_t *model,
    const S_return *ret,
    abstract_interp_t &cur_traversal,
    void *arg);

typedef source_deriver_state_t<int, int_unique_t> int_source_deriver_state_t;
typedef source_deriver_state_t<const Expression *, expr_unique_t> ast_source_deriver_state_t;

extern template class source_deriver_state_t<int, int_unique_t>;
extern template class source_deriver_state_t<const Expression *, expr_unique_t>;

#endif // SOURCE_DERIVER_STATE_HPP
