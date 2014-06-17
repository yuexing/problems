/**
 * checker-syn-store-impl.hpp
 *
 * Implementation of checker-syn-store.
 * If using a common instantiation (int_checker_syn_store_t,
 * ast_checker_syn_store_t), you don't need this header.
 *
 * (c) 2012-2014 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef AST_CHECKER_SYN_STORE_IMPL_HPP
#define AST_CHECKER_SYN_STORE_IMPL_HPP

#include "checker-syn-store.hpp"
#include "syn-store-impl.hpp"

#define STORE_CLASS checker_syn_store_t<T, MakeValueUnique, W, Cache, Copy>

#define STORE_SCOPE(type)                       \
    template<class T,                           \
        class MakeValueUnique,                  \
        class W,                                \
        class Cache,                            \
        class Copy >                            \
    type STORE_CLASS                            \

#define STORE_SCOPE_RET_SCOPED(type)            \
    STORE_SCOPE(typename STORE_CLASS::type)

STORE_SCOPE()::checker_syn_store_t(mc_arena a, sm_t *sm) :
              compose_state<this_store_t, err_store_t>(a, sm)
{
    // Can only be created with a checker_t as sm
    // ctor accepts a regular sm for composte_state compatibility
    cond_assert(dynamic_cast<checker_t *>(sm));
}

STORE_SCOPE(err_store_t const *)::error_store() const {
    return this->second();
}

STORE_SCOPE(err_store_t *)::error_store() {
    return this->second();
}

STORE_SCOPE_RET_SCOPED(this_store_t const *)::value_store() const {
    return this->first();
}

STORE_SCOPE_RET_SCOPED(this_store_t *)::value_store() {
    return this->first();
}

STORE_SCOPE_RET_SCOPED(const_iterator)::begin() const { return value_store()->begin(); }
STORE_SCOPE_RET_SCOPED(const_iterator)::end() const { return value_store()->end(); }
STORE_SCOPE_RET_SCOPED(iterator)::begin() { return value_store()->begin(); }
STORE_SCOPE_RET_SCOPED(iterator)::end() { return value_store()->end(); }

STORE_SCOPE(bool)::empty() const {
    return value_store()->empty();
}

STORE_SCOPE_RET_SCOPED(const_error_iterator)::error_begin() const { return error_store()->begin(); }
STORE_SCOPE_RET_SCOPED(const_error_iterator)::error_end() const { return error_store()->end(); }
STORE_SCOPE_RET_SCOPED(error_iterator)::error_begin() { return error_store()->begin(); }
STORE_SCOPE_RET_SCOPED(error_iterator)::error_end() { return error_store()->end(); }

STORE_SCOPE(bool)::has(const Expression *t, const T& val) const
{
    return value_store()->has(t, val);
}

STORE_SCOPE(bool)::contains(const Expression *t) const {
    return value_store()->contains(t);
}

STORE_SCOPE(void)::insert(const Expression *t,
                          const T& state,
                          err_t* error,
                          bool use_tree)
{
    value_store()->insert(t, state, use_tree);
    if (error) {
        error_store()->insert(t, error, use_tree);
    }
}

STORE_SCOPE(void)::reset_insert(const Expression *t, const T& state, err_t *error)
{
    value_store()->reset_insert(t, state);
    if(error) {
        error_store()->reset_insert(t, error);
    }
}

STORE_SCOPE(size_t)::equiv_class_count(int equiv_class) const {
    return value_store()->equiv_class_count(equiv_class);
}

STORE_SCOPE(int)::get_equiv(const Expression *t) const {
    return value_store()->get_equiv(t);
}

STORE_SCOPE(bool)::is_equiv(const Expression *t1, const Expression *t2) const {
    return value_store()->is_equiv(t1,t2);
}

STORE_SCOPE(const Expression *)::get_tree_from_equiv(int eq) const {
    return value_store()->get_tree_from_equiv(eq);
}

STORE_SCOPE(err_t *)::create_error
(abstract_interp_t &cur_traversal) {
    return generic_create_error
        (this->get_arena(),
         static_cast<checker_t *>(this->sm),
         /*path_sensitive*/true,
         cur_traversal);
}

STORE_SCOPE(bool)::assign_unknown(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->assign_unknown(t, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
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
        return assign_unknown(
            assign->target,
            removedExprs);
    }
    err_t *err = find_error(assign->src);
    if(err) {
        err->add_assign_event(
            tag,
            assign);
    }
    this->assign(assign->target, assign->src, removedExprs);
    return true;
}

STORE_SCOPE(bool)::unscope(
    const E_variable *t,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->unscope(t, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(err_t *)::find_error(const Expression *t) const
{
    typename err_store_t::const_iterator it = error_store()->find(t);
    if (it == error_end()) {
        return NULL;
    }
    return it.valueRef();
}

STORE_SCOPE(void)::commit_and_erase_error
(const Expression *t,
 abstract_interp_t &cur_traversal) {
    err_t *err = find_error(t);
    if(err) {
        static_cast<checker_t *>(this->sm)->commit_error(err, cur_traversal);
    }
    erase(t);
}

STORE_SCOPE(bool)::erase(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->erase(t, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(void)::erase(iterator i)
{
    error_store()->erase(i.key());
    value_store()->erase(i);
}

STORE_SCOPE(bool)::reset(const Expression *t)
{
    if(value_store()->reset(t)) {
        error_store()->reset(t);
        return true;
    }
    return false;
}

STORE_SCOPE(void)::reset(iterator i)
{
    error_store()->reset(i.key());
    value_store()->reset(i);
}

STORE_SCOPE(bool)::erase_all(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->erase_all(t, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(bool)::reset_all(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    vector<const Expression *> exprsRemovedHere;
    if(!value_store()->reset_all(t, /*reflexive*/true, &exprsRemovedHere)) {
        return false;
    }
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
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
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(bool)::reset_if_expr(
    bool (*predicate)(const Expression *, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs)
{
    typename this_store_t::IfExpr e = {predicate, arg};
    return reset_if(this_store_t::if_expr, &e, removedExprs);
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
    foreach(i, exprsRemovedHere) {
        error_store()->reset(*i);
    }
    if(removedExprs) {
        append_all(*removedExprs, exprsRemovedHere);
    }
    return true;
}

STORE_SCOPE(bool)::erase_if_expr(
    bool (*predicate)(const Expression *, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs)
{
    typename this_store_t::IfExpr e = {predicate, arg};
    return erase_if(this_store_t::if_expr, &e, removedExprs);
}

STORE_SCOPE(bool)::erase_pattern(
    AST_patterns::ExpressionPattern &pat,
    vector<const Expression *> *removedExprs)
{
    return erase_if_expr(pattern_matches_expr,
                         pat.get_address(), removedExprs);
}

STORE_SCOPE(bool)::reset_pattern(
    AST_patterns::ExpressionPattern &pat,
    vector<const Expression *> *removedExprs)
{
    return reset_if_expr(pattern_matches_expr,
                         pat.get_address());
}

STORE_SCOPE_RET_SCOPED(const_iterator)::find(const Expression *t) const {
    return value_store()->find(t);
}
STORE_SCOPE_RET_SCOPED(iterator)::find(const Expression *t) {
    return value_store()->find(t);
}

STORE_SCOPE(bool)::assign(
    const Expression *t,
    const Expression *t2,
    vector<const Expression *> *removedExprs)
{
    if(!value_store()->assign(t, t2, removedExprs)) {
        return false;
    }
    error_store()->assign(t, t2);
    return true;
}

STORE_SCOPE(void)::merge_equiv(const Expression *t1, const Expression *t2) {
    value_store()->merge_equiv(t1, t2);
    error_store()->merge_equiv(t1, t2);
}

STORE_SCOPE(err_t *)::assign_copy_error(const Expression *t1, const Expression *t2) {
    value_store()->assign(t1, t2);

    if (err_t* err = find_error(t2)) {
        err = err->clone(this->get_arena());
        error_store()->insert(t1, err);
        return err;
    } else {
        sup_abort("Expected an error object");
        return NULL;
    }
}

STORE_SCOPE(state_t *)::clone(mc_arena a) const
{
    this_checker_store_t* dup = new (a) this_checker_store_t(a, this->sm, *this);
    return dup;
}

STORE_SCOPE(void)::normalize()
{
    // normalize the syn_store_t;
    value_store()->normalize();

    // remove from the error_syn_store_t based on our results
    // of normalizing the syn_store_t
    error_iterator it = error_begin();
    while (it != error_end()) {
        // remove if not in the syn_store_t
        if (find(it.key()) == end()) {
            error_store()->reset(it++);
        } else {
            ++it;
        }
    }
}

STORE_SCOPE(void)::clear() {
    value_store()->clear();
    error_store()->clear();
}

STORE_SCOPE()::checker_syn_store_t(mc_arena a, sm_t *sm, const this_checker_store_t &o)
              : compose_state<this_store_t, err_store_t>(a, sm, o)
{ }

#undef STORE_SCOPE
#undef STORE_SCOPE_RET_SCOPED
#undef STORE_CLASS

#endif // ifndef __CHECKER_SYN_STORE__H__
