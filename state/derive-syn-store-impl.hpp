/**
 * derive-syn-store-impl.hpp
 *
 * Implementation of derive-syn-store.
 * If using a common instantiation, you don't need this header.
 *
 * (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef AST_DERIVE_STORE_IMPL_HPP
#define AST_DERIVE_STORE_IMPL_HPP

#include "derive-syn-store.hpp"
#include "syn-store-impl.hpp"

#define STORE_CLASS legacy_derive_syn_store_t<T, MakeValueUnique, W, Cache, Copy>

#define STORE_SCOPE(type)                       \
    template<class T,                           \
        class MakeValueUnique,                  \
        class W,                                \
        class Cache,                            \
        class Copy>                             \
    type STORE_CLASS

#define STORE_SCOPE_RET_SCOPED(type)            \
    STORE_SCOPE(typename STORE_CLASS::type)

STORE_SCOPE()::legacy_derive_syn_store_t(mc_arena a, sm_t *sm) :
              compose_state3<this_store_t, model_store_t, ignored_ifaces_set_t>(
                  a,
                  sm,
                  new(*a) this_store_t(a, sm),
                  new (*a) model_store_t(a, sm),
                  new (*a) ignored_ifaces_set_t(
                      a,
                      sm,
                      /*continue_if_new_object*/false,
                      "Ignored interfaces")
              )
{
    cond_assert(dynamic_cast<derive_sm_t *>(sm));
}

STORE_SCOPE_RET_SCOPED(this_store_t const *)::value_store() const { return this->first(); }
STORE_SCOPE_RET_SCOPED(this_store_t *)::value_store() { return this->first(); }

STORE_SCOPE_RET_SCOPED(const_iterator)::begin() const { return value_store()->begin(); }
STORE_SCOPE_RET_SCOPED(const_iterator)::end() const { return value_store()->end(); }
STORE_SCOPE_RET_SCOPED(iterator)::begin() { return value_store()->begin(); }
STORE_SCOPE_RET_SCOPED(iterator)::end() { return value_store()->end(); }

STORE_SCOPE(bool)::empty() const {
    return value_store()->empty();
}

STORE_SCOPE_RET_SCOPED(iterator)::find(const Expression *t) {
    return value_store()->find(t);
}


STORE_SCOPE(model_store_t const *)::model_store() const { return this->second(); }
STORE_SCOPE(model_store_t *)::model_store() { return this->second(); }
STORE_SCOPE(model_store_t *)::get_model_store() { return model_store(); } // legacy

STORE_SCOPE_RET_SCOPED(const_model_iterator)::model_begin() const { return model_store()->begin(); }
STORE_SCOPE_RET_SCOPED(const_model_iterator)::model_end() const { return model_store()->end(); }
STORE_SCOPE_RET_SCOPED(model_iterator)::model_begin() { return model_store()->begin(); }
STORE_SCOPE_RET_SCOPED(model_iterator)::model_end() { return model_store()->end(); }

STORE_SCOPE(ignored_ifaces_set_t::set_type &)::ignored_ifaces() {
    return this->third()->s;
}

STORE_SCOPE(bool)::has(const Expression *t, const T& val) const
{
    return value_store()->has(t, val);
}

STORE_SCOPE(bool)::contains(const Expression *t) const {
    return value_store()->contains(t);
}

STORE_SCOPE(void)::insert(const Expression *t,
                          const T& state,
                          model_t* model,
                          bool use_tree)
{
    value_store()->insert(t, state, use_tree);
    if (model) {
        model_store()->insert(t, model, use_tree);
    }
}

STORE_SCOPE(model_t *)::create_sink_model(const Expression *t,
                                          abstract_interp_t &cur_traversal)
{
    return get_derive_sm()->create_sink_model(
        this->get_arena(),
        t,
        cur_traversal);
}

STORE_SCOPE(bool)::is_ignored(const action_interface_t *iface)
{
    if(::contains(ignored_ifaces(), iface)) {
        return true;
    }
    // If the iface is p->field, see if *p is ignored.
    if(const field_interface_t *f =
       iface->iffield_interface_tC()) {
        // Use arena to prevent freeing
        deref_interface_t d(NULL, f->base);
        bool rv = ::contains(ignored_ifaces(), &d);
        // Prevent freeing base
        d.base = NULL;
        return rv;
    }
    return false;
}

STORE_SCOPE(bool)::is_ignored(const action_interface_t &iface)
{
    return is_ignored(&iface);
}

STORE_SCOPE(model_t *)::if_not_ignored(model_t *m) {
    if(!m || is_ignored(m->get_iface())) {
        return NULL;
    }
    return m;
}

STORE_SCOPE(model_t *)::create_model(const Expression *t,
                                     abstract_interp_t &cur_traversal)
{
    // Assume "sink" model.
    // TODO: Separate an "sink" deriver store and a "source"
    // deriver store.
    return if_not_ignored(create_sink_model(t, cur_traversal));
}

STORE_SCOPE(model_t *)::create_source_model(const Expression *t,
                                            abstract_interp_t &cur_traversal)
{
    return if_not_ignored(
        get_derive_sm()->create_source_model(
            this->get_arena(),
            t,
            cur_traversal)
    );
}

STORE_SCOPE(model_t *)::create_return_model(abstract_interp_t &cur_traversal)
{
    // No reason to ignore the "return" interface; do not call
    // if_not_ignored.
    return get_derive_sm()->
        create_model(this->get_arena(),
                     return_interface_t(),
                     cur_traversal);
}

STORE_SCOPE(model_t *)::create_function_model(abstract_interp_t &cur_traversal)
{
    // For compatibility, make these path-sensitive.
    // Like create_return_model(), no point in checking if_not_ignored.
    return get_derive_sm()->
        create_model(this->get_arena(),
                     function_interface_t(),
                     cur_traversal);
}

STORE_SCOPE(bool)::is_sink_iface(
    const Expression *expr,
    abstract_interp_t &cur_traversal)
{
    const event_list_t *alias_events;
    return find_sink_iface(expr, cur_traversal, alias_events) != NULL;
}

STORE_SCOPE(const action_interface_t *)::find_sink_iface(
    const Expression *expr,
    abstract_interp_t &cur_traversal,
    const event_list_t *&alias_events)
{
    const action_interface_t *iface =
        get_derive_sm()->find_sink_iface(expr, cur_traversal, alias_events);
    if(!iface || is_ignored(iface)) {
        return NULL;
    }
    return iface;
}

STORE_SCOPE(const action_interface_t *)::find_source_iface(
    const Expression *expr,
    abstract_interp_t &cur_traversal,
    const event_list_t *&alias_events)
{
    // TODO: source interfaces are probably never "ignored".
    // In any case, legacy_derive_syn_store_t is meant to be replaced
    // with source_deriver_state_t and sink_deriver_state_t
    // eventually.
    const action_interface_t *iface =
        get_derive_sm()->find_source_iface(expr, cur_traversal, alias_events,
                                           /*allow_side_effect*/false);
    if(!iface || is_ignored(iface)) {
        return NULL;
    }
    return iface;
}

STORE_SCOPE(void)::assign_unknown(const Expression *t)
{
    value_store()->assign_unknown(t);
    model_store()->assign_unknown(t);
    // The interface tracker already knows about assignments, but
    // not necessarily about e.g. "write" models, so, for now,
    // include "ignore" here.
    ignore(t);
}

STORE_SCOPE(void)::ignore(const Expression *t)
{
    erase(t);
    // I'm not aware of any use cases for ignoring a source
    // interface.
    // So query sink interfaces.
    const event_list_t *alias_events;
    const action_interface_t *iface =
        static_cast<derive_sm_t *>(this->sm)->find_sink_iface(
            t, *g_evil_global_cur_abstract_interp, alias_events);
    if(iface) {
        ignored_ifaces().insert(iface);
    }
}

STORE_SCOPE(void)::unscope(const E_variable *t)
{
    value_store()->unscope(t);
    model_store()->unscope(t);
}

STORE_SCOPE(bool)::erase_all(const Expression *t)
{
    vector<const Expression *> removedExprs;
    if(!value_store()->erase_all(t, &removedExprs)) {
        return false;
    }
    foreach(i, removedExprs) {
        model_store()->reset(*i);
    }
    return true;
}

STORE_SCOPE(bool)::reset_all(const Expression *t)
{
    vector<const Expression *> removedExprs;
    if(!value_store()->reset_all(t, /*reflexive*/true, &removedExprs)) {
        return false;
    }
    foreach(i, removedExprs) {
        model_store()->reset(*i);
    }
    return true;
}

STORE_SCOPE(void)::clear() {
    value_store()->clear();
    model_store()->clear();
}

STORE_SCOPE(model_t *)::find_model(const Expression *t)
{
    typename model_store_t::iterator it = model_store()->find(t);
    if (it == model_end()) {
        return NULL;
    }
        
    return it.valueRef();
}

STORE_SCOPE(bool)::erase(const Expression *t)
{
    vector<const Expression *> removedExprs;
    if(value_store()->erase(t, &removedExprs)) {
        foreach(i, removedExprs) {
            model_store()->reset(*i);
        }
        return true;
    }
    return false;
}

STORE_SCOPE(void)::erase(iterator i)
{
    model_store()->erase(i.key());
    value_store()->erase(i);
}

STORE_SCOPE(bool)::reset(const Expression *t)
{
    if(value_store()->reset(t)) {
        model_store()->reset(t);
        return true;
    }
    return false;
}

STORE_SCOPE(bool)::reset_if(
    bool (*predicate)(const typename this_store_t::iterator &, void *arg),
    void *arg)
{
    vector<const Expression *> removedExprs;
    if(!value_store()->reset_if(predicate, arg, &removedExprs)) {
        return false;
    }
    foreach(i, removedExprs) {
        model_store()->reset(*i);
    }
    return true;
}

STORE_SCOPE(bool)::reset_if_expr(
    bool (*predicate)(const Expression *, void *arg),
    void *arg)
{
    typename this_store_t::IfExpr e = {predicate, arg};
    return reset_if(this_store_t::if_expr, &e);
}

STORE_SCOPE(bool)::erase_if(
    bool (*predicate)(const typename this_store_t::iterator &, void *arg),
    void *arg)
{
    vector<const Expression *> removedExprs;
    if(!value_store()->erase_if(predicate, arg, NULL, &removedExprs)) {
        return false;
    }
    foreach(i, removedExprs) {
        // Still call "reset"; we just want to remove the same keys.
        model_store()->reset(*i);
    }
    return true;
}

STORE_SCOPE(bool)::erase_if_expr(
    bool (*predicate)(const Expression *, void *arg),
    void *arg)
{
    typename this_store_t::IfExpr e = {predicate, arg};
    return erase_if(this_store_t::if_expr, &e);
}

STORE_SCOPE(bool)::erase_pattern(
    AST_patterns::ExpressionPattern &pat) {
    return erase_if_expr(pattern_matches_expr,
                         pat.get_address());
}

STORE_SCOPE(bool)::reset_pattern(
    AST_patterns::ExpressionPattern &pat) {
    return reset_if_expr(pattern_matches_expr,
                         pat.get_address());
}

STORE_SCOPE(void)::assign(const Expression *t, const Expression *t2) {
    value_store()->assign(t, t2);
    model_store()->assign(t, t2);
}

STORE_SCOPE(void)::merge_equiv(const Expression *t1, const Expression *t2) {
    value_store()->merge_equiv(t1, t2);
    model_store()->merge_equiv(t1, t2);
}

STORE_SCOPE(state_t *)::clone(mc_arena a) const
{
    this_legacy_derive_syn_store_t* dup =
        new (a) this_legacy_derive_syn_store_t(a, this->sm, *this);
    return dup;
}

STORE_SCOPE(void)::normalize()
{
    // normalize the syn_store_t;
    value_store()->normalize();
        
    // remove from the model_store_t based on our results
    // of normalizing the syn_store_t
    model_iterator it = model_begin();
    while (it != model_end()) {
        // remove if not in the syn_store_t
        if (find(it.key()) == end()) {
            model_store()->reset(it++);
        } else {
            ++it;
        }
    }
}

STORE_SCOPE(size_t)::equiv_class_count(int equiv_class) {
    return value_store()->equiv_class_count(equiv_class);
}

STORE_SCOPE()::legacy_derive_syn_store_t(mc_arena a, sm_t *sm, const this_legacy_derive_syn_store_t &o)
              : compose_state3<this_store_t, model_store_t, ignored_ifaces_set_t>(a, sm, o)
{ }

STORE_SCOPE(derive_sm_t *)::get_derive_sm() const {
    return static_cast<derive_sm_t *>(this->sm);
}

#undef STORE_SCOPE
#undef STORE_SCOPE_RET_SCOPED
#undef STORE_CLASS

#endif // AST_DERIVE_STORE_IMPL_HPP
