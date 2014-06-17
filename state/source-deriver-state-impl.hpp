/**
 * source-deriver-store-impl.hpp
 *
 * Implementation of source-deriver-store.
 * If using a common instantiation, you don't need this header.
 *
 * (c) 2012-2014 Coverity, Inc. All rights reserved worldwide.
 **/
#ifndef SOURCE_DERIVER_STATE_IMPL_HPP
#define SOURCE_DERIVER_STATE_IMPL_HPP

#include "source-deriver-state.hpp"
#include "syn-store-impl.hpp"
#include "analysis/sm/derive-sm.hpp"

#define STORE_CLASS source_deriver_state_t<T, MakeValueUnique, W, Cache, Copy>

#define STORE_SCOPE(type)                       \
    template<class T,                           \
        class MakeValueUnique,                  \
        class W,                                \
        class Cache,                            \
        class Copy>                             \
    type STORE_CLASS

#define STORE_SCOPE_RET_SCOPED(type)            \
    STORE_SCOPE(typename STORE_CLASS::type)

STORE_SCOPE()::source_deriver_state_t(
    mc_arena a,
    sm_t *sm,
    bool allowOutputParameterInterface) :
              super(
                  a,
                  sm,
                  new (*a) this_store_t(a, sm),
                  new (*a) model_store_t(a, sm)
              ),
              iface_to_model(a),
              allowOutputParameterInterface(allowOutputParameterInterface)
{
    cond_assert(dynamic_cast<derive_sm_t *>(sm));
}

STORE_SCOPE(void)::write_as_text(StateWriter& out) const {
    super::write_as_text(out);
    SW_WRITE_BLOCK(out, "Interface models:");
    foreach(i, iface_to_model) {
        out << *i->first
            << " -> "
            << i->second.equiv_class
            << " : ";
        i->second.m->write_as_text(out);
    }
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


STORE_SCOPE(const model_store_t *)::model_store() const { return this->second(); }
STORE_SCOPE(model_store_t *)::model_store() { return this->second(); }
STORE_SCOPE(model_store_t *)::get_model_store() { return model_store(); } // legacy

STORE_SCOPE_RET_SCOPED(const_model_iterator)::model_begin() const { return model_store()->begin(); }
STORE_SCOPE_RET_SCOPED(const_model_iterator)::model_end() const { return model_store()->end(); }
STORE_SCOPE_RET_SCOPED(model_iterator)::model_begin() { return model_store()->begin(); }
STORE_SCOPE_RET_SCOPED(model_iterator)::model_end() { return model_store()->end(); }

STORE_SCOPE(bool)::has(const Expression *t, const T& val) const
{
    return value_store()->has(t, val);
}

STORE_SCOPE(bool)::contains(const Expression *t) const {
    return value_store()->contains(t);
}

STORE_SCOPE(opt_uint)::insert(
    const Expression *t,
    const T& state,
    model_t* model,
    abstract_interp_t &cur_traversal)
{
    value_store()->insert(t, state);
    if (model) {
        int ec;
        model_store()->insert_and_get_equiv_class(t, model, ec);
        assign_model_to_iface(
            t,
            model,
            ec,
            /*allow_side_effect*/get_derive_sm()->allow_side_effect_source(),
            /*doClone*/false,
            cur_traversal);
        return ec;
    }
    return opt_uint();
}

STORE_SCOPE(model_t *)::create_return_model(abstract_interp_t &cur_traversal)
{
    return get_derive_sm()->
        create_model(this->get_arena(),
                     return_interface_t(),
                     cur_traversal);
}

STORE_SCOPE(model_t *)::insert_new_model(
    const Expression *t,
    const T &state,
    abstract_interp_t &cur_traversal)
{
    model_t *m = get_derive_sm()->
        create_noiface_model(this->get_arena(), cur_traversal);
    insert(t, state, m, cur_traversal);
    return m;
}

STORE_SCOPE(model_t *)::create_function_model(abstract_interp_t &cur_traversal)
{
    // For compatibility, make these path-sensitive.
    return get_derive_sm()->
        create_model(this->get_arena(),
                     function_interface_t(),
                     cur_traversal);
}

STORE_SCOPE(const action_interface_t *)::find_iface(
    const Expression *expr,
    abstract_interp_t &cur_traversal,
    const event_list_t *&alias_events,
    bool allow_side_effect)
{
    return get_derive_sm()->find_source_iface(
        expr,
        cur_traversal,
        alias_events,
        allow_side_effect);
}

STORE_SCOPE(bool)::assign_unknown(
    const Expression *t,
    abstract_interp_t &cur_traversal,
    bool allow_side_effect,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->reset_all(t, /*reflexive*/true, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        model_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    reassign_iface(t, allow_side_effect, cur_traversal);
    return true;
}

STORE_SCOPE(bool)::handle_assign(
    const char *tag,
    const E_assign *assign,
    abstract_interp_t &cur_traversal,
    vector<const Expression *> *removedExprs)
{
    if(assign->op != BIN_ASSIGN) {
        return false;
    }
    LET(i, find(assign->src));
    if(i == end()) {
        return assign_unknown(assign->target, cur_traversal, removedExprs);
    }
    model_t *m = find_model(assign->src);
    if(m) {
        m->add_assign_event(
            tag,
            assign);
    }
    this->assign(assign->target, assign->src, cur_traversal, removedExprs);
    return true;
}

STORE_SCOPE(void)::reassign_iface(
    const Expression *e,
    bool allow_side_effect,
    abstract_interp_t &cur_traversal) {
    const event_list_t *alias_events;
    if(const action_interface_t *iface =
       find_iface(
           e,
           cur_traversal,
           alias_events,
           allow_side_effect)) {
        iface_to_model.erase(iface);
        const deref_interface_t *d;
        if((d = iface->ifderef_interface_tC()) != NULL
           &&
           e->type->isclass_type_t()) {
            // If we write *p, we implicitly write p->field.
            for(LET(i, iface_to_model.begin());
                i != iface_to_model.end();
                /**/) {
                const field_interface_t *fi;
                if((fi = i->first->iffield_interface_tC()) != NULL
                   &&
                   *fi->base == *d->base) {
                    iface_to_model.erase(i++);
                } else {
                    ++i;
                }
            }
        }
    }
}

STORE_SCOPE(bool)::handleNewExpr(const Expression *e, abstract_interp_t &cur_traversal) {
    const E_new *nw = e->ifE_newC();
    if(!nw) {
        return false;
    }
    const E_variable *newVar = nw->getNewVarExpr();
    if(newVar) {
        assign(nw, newVar, cur_traversal);
        reset(newVar);
    }
    return true;
}

STORE_SCOPE(void)::handleReturn(
    const S_return *ret,
    abstract_interp_t &cur_traversal,
    void (*add_event)(
        model_t *events,
        const S_return *ret,
        abstract_interp_t &cur_traversal,
        void *arg),
    void *arg)
{
    const Expression *expr = ret->expr;
    if(!expr) {
        return;
    }
    model_t *m = find_model(expr);
    if(!m) {
        return;
    }
    // Clone the model before committing, so that if it's
    // also associated with another interface, it's not
    // double-committed.
    stack_allocated_arena_t ar("ret model");
    model_t *return_model = m->clone(&ar);
    return_model->set_iface(return_interface_t());
    add_event(return_model, ret, cur_traversal, arg);
    get_derive_sm()->commit_model(return_model, cur_traversal);
}

STORE_SCOPE(bool)::unscope(
    const E_variable *var,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->unscope(var, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        model_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(bool)::remove_transients(
    vector<const Expression *> *removedExprs)
{
    return reset_pattern(transient, removedExprs);
}

STORE_SCOPE(bool)::erase_all(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->erase_all(t, &exprsRemovedHere)) {
        return false;
    }
    bool need_normalize = false;
    foreach(i, exprsRemovedHere) {
        if(model_store()->reset(*i)) {
            need_normalize = true;
        }
    }
    if(need_normalize) {
        normalize_iface_to_model();
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(void)::clear() {
    value_store()->clear();
    model_store()->clear();
    iface_to_model.clear();
}

STORE_SCOPE(model_t *)::find_model(const Expression *t)
{
    typename model_store_t::iterator it = model_store()->find(t);
    if (it == model_end()) {
        return NULL;
    }

    return it.value();
}

STORE_SCOPE(bool)::erase(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    if(!value_store()->erase(t, removedExprs)) {
        return false;
    }
    opt_uint model_equiv_class = model_store()->find_equiv_class(t);
    if(model_equiv_class.is_present()) {
        model_store()->erase(t);
        normalize_iface_to_model();
    }
    return true;
}

STORE_SCOPE(bool)::reset(const Expression *t)
{
    if(!model_store()->reset(t)) {
        return false;
    }
    value_store()->reset(t);
    return true;
}

STORE_SCOPE(bool)::reset_if(
    bool (*predicate)(const typename this_store_t::iterator &, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->reset_if(predicate, arg, &exprsRemovedHere)) {
        return false;
    }
    bool need_normalize = false;
    foreach(i, exprsRemovedHere) {
        if(model_store()->reset(*i)) {
            need_normalize = true;
        }
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    if(need_normalize) {
        normalize_iface_to_model();
    }
    return true;
}

STORE_SCOPE(bool)::reset_if_expr(
    bool (*predicate)(const Expression *, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs)
{
    typename this_store_t::IfExpr e = { predicate, arg};
    return reset_if(
        this_store_t::if_expr,
        &e,
        removedExprs);
}

STORE_SCOPE(bool)::reset_pattern(
    AST_patterns::ExpressionPattern &pat,
    vector<const Expression *> *removedExprs)
{
    return reset_if_expr(pattern_matches_expr, pat.get_address(), removedExprs);
}

STORE_SCOPE(bool)::erase_if(
    bool (*predicate)(const typename this_store_t::iterator &, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->erase_if(predicate, arg, NULL, &exprsRemovedHere)) {
        return false;
    }
    bool need_normalize = false;
    foreach(i, exprsRemovedHere) {
        if(model_store()->reset(*i)) {
            need_normalize = true;
        }
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    if(need_normalize) {
        normalize_iface_to_model();
    }
    return true;
}

STORE_SCOPE(bool)::erase_if_expr(
    bool (*predicate)(const Expression *, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs)
{
    typename this_store_t::IfExpr e = { predicate, arg};
    return erase_if(
        this_store_t::if_expr,
        &e,
        removedExprs);
}

STORE_SCOPE(bool)::erase_pattern(
    AST_patterns::ExpressionPattern &pat,
    vector<const Expression *> *removedExprs)
{
    return erase_if_expr(pattern_matches_expr, pat.get_address(), removedExprs);
}

STORE_SCOPE(bool)::assign(
    const Expression *t,
    const Expression *t2,
    abstract_interp_t &cur_traversal,
    bool allow_side_effect,
    vector<const Expression *> *removedExprs) {
    if(!value_store()->assign(t, t2, removedExprs)) {
        return false;
    }
    model_store()->assign(t, t2);
    reassign_iface(t, /*erase_value_models*/allow_side_effect, cur_traversal);
    // See if "t2" has a model associated with it, and "t" is an
    // interface.
    LET(i, model_store()->find(t2));
    if(i != model_store()->end()) {
        assign_model_to_iface(
            t,
            i.value(),
            i.equiv_class(),
            allow_side_effect,
            /*doClone*/true,
            cur_traversal);
    }
    return true;
}

STORE_SCOPE(void)::assign_model_to_iface(
    const Expression *ifaceExpr,
    model_t *m,
    int equiv_class,
    bool allow_side_effect,
    bool doClone,
    abstract_interp_t &cur_traversal) {
    if(!allowOutputParameterInterface) {
        return;
    }
    const event_list_t *alias_events;
    if(const action_interface_t *iface =
       find_iface(
           ifaceExpr,
           cur_traversal,
           alias_events,
           allow_side_effect)) {
        if(doClone) {
            m = m->clone(this->get_arena());
        }
        m->add_events(alias_events);
        iface_to_model[iface] = model_and_equiv_class_t(m, equiv_class);
    }
}

STORE_SCOPE(state_t *)::clone(mc_arena a) const
{
    this_derive_syn_store_t* dup =
        new (a) this_derive_syn_store_t(a, this->sm, *this);
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
    normalize_iface_to_model();
}

STORE_SCOPE(size_t)::equiv_class_count(int equiv_class) {
    return value_store()->equiv_class_count(equiv_class);
}

STORE_SCOPE(void)::on_fn_eop(abstract_interp_t &cur_traversal)
{
    commit_all_models(cur_traversal);
}

STORE_SCOPE(void)::discard_all_models()
{
    iface_to_model.clear();
}

STORE_SCOPE(void)::commit_all_models(abstract_interp_t &cur_traversal)
{
    derive_sm_t *sm = get_derive_sm();
    foreach(i, iface_to_model) {
        model_t *m = i->second.m;
        m->set_iface(*i->first);
        sm->commit_model(m, cur_traversal);
    }
    iface_to_model.clear();
}

STORE_SCOPE()::source_deriver_state_t(
    arena_t *a,
    sm_t *sm,
    const this_derive_syn_store_t &o)
              : compose_state<this_store_t, model_store_t>(a, sm, o),
              iface_to_model(o.iface_to_model, a),
              allowOutputParameterInterface(o.allowOutputParameterInterface)
{
    foreach(i, iface_to_model) {
        i->second.m = i->second.m->clone(a);
    }
}

STORE_SCOPE(derive_sm_t *)::get_derive_sm() const {
    return static_cast<derive_sm_t *>(this->sm);
}

STORE_SCOPE(void)::normalize_iface_to_model() {
    const model_store_t::equiv_map &valueof =
        model_store()->get_valueof_map();
    // We need to erase any mapping to any equivalence class that
    // was removed from iface_to_model.
    for(LET(i, iface_to_model.begin()); i != iface_to_model.end(); /**/) {
        if(!valueof.is_mapped(i->second.equiv_class)) {
            iface_to_model.erase(i++);
        } else {
            ++i;
        }
    }
}

#endif // SOURCE_DERIVER_STATE_HPP
