/**
 * primitive-syn-store-impl.hpp
 *
 * Implementation of primitive-syn-store.
 * If using a common instantiation, you don't need this header.
 *
 * (c) 2004-2014 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef PRIMITIVE_SYN_STORE_IMPL_HPP
#define PRIMITIVE_SYN_STORE_IMPL_HPP

#include "primitive-syn-store.hpp"
#include "analysis/ast/symbols/field.hpp" // get_owner_class()
#include "libs/time/activity-path-timer.hpp" // DEF_FREQUENT_ACTIVITY

#include "libs/containers/map.hpp"
// See store-impl.hpp for a description of these macros
#define STORE_CLASS primitive_syn_store_t<T, Copy>

#define STORE_SCOPE(type)                       \
    template<class T,                           \
        class Copy>                             \
    type STORE_CLASS

#define STORE_SCOPE_RET_SCOPED(type)            \
    STORE_SCOPE(typename STORE_CLASS::type)

// Macros to define iterator methods.
// Work similarly to the "STORE" versions.
#define ITER_CLASS STORE_CLASS::iter<It, store_type, eit, Value>

#define ITER_SCOPE(type)                        \
    template<class T,                           \
        class Copy>                             \
    template <class It,                         \
        class store_type,                       \
        class eit,                              \
        typename Value>                         \
    type ITER_CLASS

ITER_SCOPE()::iter() : store(0) { }

ITER_SCOPE()::iter(It p, store_type *store)
             : p(p), store(store) { }

ITER_SCOPE(ITER_CLASS &)
::operator=(const iter& other) {
    p = other.p;
    store = other.store;
    return *this;
}

ITER_SCOPE(Expression const *)::key() const
{ return (*p).first; }

ITER_SCOPE(int)::equiv_class() const
{ return (*p).second; }

ITER_SCOPE(int)::ref_count() const
{ return store->existing_equiv_class_ref(equiv_class()).refCount; }

ITER_SCOPE(T)::value() const
{ return valueRef(); }

ITER_SCOPE(Value &)::valueRef() const
{
    // This is constant time
    return store->existing_valueof_ref(equiv_class());
}

ITER_SCOPE(ITER_CLASS &)::operator++()
{ ++p; return *this; }

ITER_SCOPE(ITER_CLASS)::operator++(int x)
{ iter rv = *this; operator++(); return rv; }

ITER_SCOPE(ITER_CLASS &)::operator--()
{ --p; return *this; }

ITER_SCOPE(ITER_CLASS)::operator--(int x)
{ iter rv = *this; operator--(); return rv; }

ITER_SCOPE(bool)::operator==(const iter &other) const
{
    return store == other.store && p == other.p;
}

ITER_SCOPE(bool)::operator!=(const iter &other) const
{
    return store != other.store || p != other.p;
}

#define VALUE_ITER_CLASS STORE_CLASS::value_iter<eit, Value>

#define VALUE_ITER_SCOPE(type)                  \
    template<class T,                           \
        class Copy>                             \
    template <typename eit,                     \
        typename Value>                         \
    type VALUE_ITER_CLASS

VALUE_ITER_SCOPE()::value_iter() { }

VALUE_ITER_SCOPE()::value_iter(eit p)
             : p(p) { }

VALUE_ITER_SCOPE(VALUE_ITER_CLASS &)
::operator=(const value_iter& other) {
    p = other.p;
    return *this;
}

VALUE_ITER_SCOPE(Value &)::valueRef() const
{ return p.value().value; }

VALUE_ITER_SCOPE(Value &)::operator*() const
{ return valueRef(); }

VALUE_ITER_SCOPE(Value *)::operator->() const
{ return &valueRef(); }

VALUE_ITER_SCOPE(T)::value() const
{ return valueRef(); }

VALUE_ITER_SCOPE(int)::equiv_class() const
{ return p.key(); }

VALUE_ITER_SCOPE(VALUE_ITER_CLASS &)::operator++()
{ ++p; return *this; }

VALUE_ITER_SCOPE(VALUE_ITER_CLASS)::operator++(int)
{ value_iter rv = *this; operator++(); return rv; }

VALUE_ITER_SCOPE(bool)::operator==(const value_iter &other) const
{
    return p == other.p;
}

VALUE_ITER_SCOPE(bool)::operator!=(const value_iter &other) const
{
    return p != other.p;
}

STORE_SCOPE()::primitive_syn_store_t(arena_t *a, sm_t *sm) :
              equiv_class(a),
              valueof(*a) {}

STORE_SCOPE()::primitive_syn_store_t(arena_t *a, const this_store_t &o) :
              equiv_class(a),
              valueof(*a)
{
    Copy cp;
    valueof.reserve(o.valueof.last_mapped_key() + 1);
    foreach(i, o.valueof) {
        EquivClass eq = i.value();
        eq.value = cp(eq.value, a);
        valueof.insert(i.key(), eq);
    }
    foreach(i, o.equiv_class) {
        equiv_class.insert(equiv_class.end(), *i);
    }
}

STORE_SCOPE(void)::normalize(vector<const Expression *> *removedExprs)
{
    DEF_FREQUENT_ACTIVITY("normalize");

    // Clear reference counts.
    foreach(i, valueof) {
        i.value().refCount = 0;
    }

    // Erase if the equivalence class doesn't have a mapping in
    // "valueof", and recompute the reference counts when there's a mapping.
    for(LET(p, equiv_class.begin()); p != equiv_class.end(); /**/) {
        if(!valueof.is_mapped(p->second)) {
            if(removedExprs) {
                removedExprs->push_back(p->first);
            }
            // This is ok w.r.t. iterator invalidation.
            equiv_class.erase(p++);
        } else {
            ++valueof.get_existing(p->second).refCount;
            ++p;
        }
    }

    // Clear equivalence classes with 0 references (although that
    // really should not happen)
    for(LET(q, valueof.begin()); q != valueof.end(); /**/) {
        if(!q.value().refCount) {
            // This is ok w.r.t. iterator invalidation.
            valueof.erase(q++);
        } else {
            ++q;
        }
    }
}

// Determine if the store is normalized.  Return false if not, true if so.
STORE_SCOPE(bool)::check_normalize() const
{
    map<int, int> refCounts;

    bool rv = true;

    foreach(p, equiv_class) {
        if(!valueof.is_mapped(p->second)) {
            cout << "Key " << *p->first
                 << " doesn't have a mapping for its equivalence class "
                 << p->second
                 << endl;
            rv = false;
        } else {
            ++refCounts[p->second];
        }
    }

    foreach(q, valueof) {
        int expectedRefct = refCounts[q.key()];
        int actualRefct = q.value().refCount;
        if(actualRefct != expectedRefct) {
            cout << "Normalize failed in valueof on value "
                 << q.key()
                 << ": Expected reference count of "
                 << expectedRefct << " but got " << actualRefct
                 << endl;
            rv = false;
        } else if(!actualRefct) {
            cout << "Normalize failed in valueof on value "
                 << q.key()
                 << ": Reference count is 0"
                 << endl;
            rv = false;
        }
    }

    if(!rv) {
        xfailure("Inconsistent state");
    }
    return rv;
}

// Set the state for the entire equivalence class
STORE_SCOPE(void)::insert(const Expression *t, const T& state, bool use_tree)
{
    int ec;
    insert_and_get_equiv_class(t, state, ec, use_tree);
}

STORE_SCOPE(void)::insert_and_get_equiv_class(
    const Expression *t,
    const T& state,
    int &ec,
    bool use_tree) {
    int_store_t::iterator it = equiv_class.find(t);
    if(it != equiv_class.end()) {
        ec = it->second;
        if(use_tree) {
            equiv_class.erase(it);
            equiv_class.insert(make_pair(t, ec));
        }
        existing_valueof_ref(ec) = state;
    } else {
        ec = valueof.first_unmapped_key();
        equiv_class[t] = ec;
        valueof.insert(ec, EquivClass(state));
    }
}

// Erase the entire equivalence class
STORE_SCOPE(bool)::erase(const Expression *t,
                         vector<const Expression *> *removedExprs)
{
    LET(i, equiv_class.find(t));
    if(i != equiv_class.end()) {
        remove_equiv_class(i->second, removedExprs);
        return true;
    }
    return false;
}

STORE_SCOPE(void)::remove_equiv_class(
    int eq_class,
    vector<const Expression *> *removedExprs)
{
    int count = valueof.get_existing(eq_class).refCount;
    for(LET(i, equiv_class.begin()); count != 0 && i != equiv_class.end(); ) {
        if(i->second == eq_class) {
            if(removedExprs) {
                removedExprs->push_back(i->first);
            }
            equiv_class.erase(i++);
            --count;
        } else {
            ++i;
        }
    }
    // Remove the equiv class
    valueof.erase(eq_class);
}

STORE_SCOPE(void)::raw_erase(value_iterator i)
{
    valueof.erase(i.p);
}

STORE_SCOPE(void)::erase(value_iterator i)
{
    remove_equiv_class(i.p.key());
}

STORE_SCOPE(void)::reset(iterator i)
{
    dec_ref_count(i.equiv_class());
    equiv_class.erase(i.p);
}

STORE_SCOPE(bool)::dec_ref_count(int equiv_class)
{
    if(!--valueof.get_existing(equiv_class).refCount) {
        valueof.erase(equiv_class);
        return true;
    }
    return false;
}

STORE_SCOPE(bool)::reset(const Expression *t)
{
    LET(i, equiv_class.find(t));
    if(i != equiv_class.end()) {
        dec_ref_count(i->second);
        equiv_class.erase(i);
        return true;
    }
    return false;
}

STORE_SCOPE(void)::erase(iterator i)
{
    int eq = i.equiv_class();
    equiv_class.erase(i.p);
    if(!dec_ref_count(eq)) {
        // Other members are present in the equivalence class,
        // that may need to be erased, too.
        remove_equiv_class(eq);
    }
}

STORE_SCOPE_RET_SCOPED(size_type)::size() const {
    return equiv_class.size();
}

STORE_SCOPE(bool)::erase_all_fields(
    const Expression *t,
    bool *pneed_normalize,
    vector<const Expression *> *removedExprs)
{
    return erase_if_expr(
        expr_contains_expr_strict,
        (void *)t,
        pneed_normalize,
        removedExprs);
}

STORE_SCOPE(bool)::erase_if(
    bool (*pred)(const iterator &, void *arg),
    void *arg,
    bool *pneed_normalize,
    vector<const Expression *> *removedExprs)
{
    bool rv = false;
    bool need_normalize = false;
    for(LET(i, this->begin());
        i != this->end();/*no increment*/) {
        // Check if we've already erased the equivalence class.
        if(need_normalize && !valueof.is_mapped(i.equiv_class())) {
            if(removedExprs) {
                removedExprs->push_back(i.key());
            }
            equiv_class.erase(i.p++);
            continue;
        }
        if(pred(i, arg)) {
            rv = true;
            if(removedExprs) {
                removedExprs->push_back(i.key());
            }
            if(--valueof.get_existing(i.equiv_class()).refCount) {
                // Other members of the equiv class may need to be
                // erased.
                need_normalize = true;
            }
            valueof.erase(i.equiv_class());
            equiv_class.erase(i.p++);
        } else {
            ++i;
        }
    }
    if(need_normalize) {
        if(pneed_normalize) {
            *pneed_normalize = need_normalize;
        } else {
            normalize(removedExprs);
        }
    }
    return rv;
}

STORE_SCOPE(bool)::erase_pattern(
    AST_patterns::ExpressionPattern &pat,
    vector<const Expression *> *removedExprs) {
    return erase_if_expr(pattern_matches_expr, pat.get_address(), NULL, removedExprs);
}

STORE_SCOPE(bool)::erase_all(const Expression *t, vector<const Expression *> *removedExprs) {
    const type_t *type = t->type;

    if(type->ispointer_type_t() || type->isclass_type_t()) {
        return erase_if_expr(
            expr_contains_expr_reflexive,
            (void *)t,
            NULL,
            removedExprs);
    } else {
        return erase(t, removedExprs);
    }
}

STORE_SCOPE(bool)::reset_all_fields(
    const Expression *t,
    vector<const Expression *> *removedExprs) {
    return reset_if_expr(expr_contains_expr_strict, (void *)t, removedExprs);
}

STORE_SCOPE(bool)::reset_if(
    bool (*pred)(const iterator &, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs) {
    bool rv = false;
    for(LET(i, this->begin());
        i != this->end();/*no increment*/) {
        if(pred(i, arg)) {
            rv = true;
            if(removedExprs) {
                removedExprs->push_back(i.key());
            }
            dec_ref_count(i.equiv_class());
            equiv_class.erase(i.p++);
        }
        else {
            ++i;
        }
    }
    return rv;
}

STORE_SCOPE(bool)::if_expr(const iterator &i, void *arg)
{
    const IfExpr &e = *static_cast<IfExpr *>(arg);
    return e.pred(i.key(), e.arg);
}

STORE_SCOPE(bool)::reset_if_expr(
    bool (*pred)(const Expression *, void *arg),
    void *arg,
    vector<const Expression *> *removedExprs) {
    IfExpr e = {pred, arg};
    return reset_if(if_expr, &e, removedExprs);
}

STORE_SCOPE(bool)::erase_if_expr(
    bool (*pred)(const Expression *, void *arg),
    void *arg,
    bool *pneed_normalize,
    vector<const Expression *> *removedExprs) {
    IfExpr e = {pred, arg};
    return erase_if(if_expr, &e, pneed_normalize, removedExprs);
}

STORE_SCOPE(bool)::reset_pattern(
    AST_patterns::ExpressionPattern &pat,
    vector<const Expression *> *removedExprs) {
    return reset_if_expr(pattern_matches_expr, pat.get_address(), removedExprs);
}

STORE_SCOPE(bool)::reset_all(
    const Expression *t,
    bool reflexive,
    vector<const Expression *> *removedExprs) {
    bool rv = false;
    if(reflexive) {
        if(reset(t)) {
            if(removedExprs) {
                removedExprs->push_back(t);
            }
            rv = true;
        }
    }
    const type_t *type = t->type;
    if(type->ispointer_type_t() || type->isclass_type_t()) {
        if(reset_all_fields(
            t,
            removedExprs)) {
            rv = true;
        }
    }
    return rv;
}

STORE_SCOPE(void)::set_all(const Expression *t, const T& val, arena_t *ar) {
    Copy cp;
    for(iterator i = begin(); i != end(); ++i) {
        if(/*current_fn_trav->*/contains_expr_no_fn(i.key(), t)) {
            // Deep-copy the value; if there's a pointer involved,
            // we don't want it to be the same for every value,
            // because the pointee might be modified.
            i.valueRef() = cp(val, ar);
        }
    }
}

STORE_SCOPE(void)::set_all_insert(const Expression *t, const T& val, arena_t *ar) {
    set_all(t, val, ar);
    insert(t, val);
}

STORE_SCOPE(bool)::assign(
    const Expression *t,
    const Expression *t2,
    vector<const Expression *> *removedExprs) {
    int equiv = get_equiv(t);
    int equiv2 = t2?get_equiv(t2):-1;
    return handle_assign(t, equiv, equiv2, removedExprs);
}

STORE_SCOPE(void)::assign_if_present(const Expression *t, const Expression *t2) {
    int equiv = get_equiv(t);
    int equiv2 = t2?get_equiv(t2):-1;
    if (equiv2 >= 0) {
        handle_assign(t, equiv, equiv2);
    }
}

STORE_SCOPE(bool)::remove_for_reassign(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    // If assigning a union field, consider we assign the union
    // itself.
    // We can still get things wrong with union fields if e.g. it's a
    // struct field off a union field, but it's much less likely to be
    // relevant, since it's uncommon to assign a struct field to mean
    // assignment of a different part of the union.
    const Expression *overwritten = t;
    const E_fieldAcc *fAcc = overwritten->ifE_fieldAccC();
    if(fAcc && fAcc->field->get_owner_class()->is_union()) {
        overwritten = fAcc->obj;
    }
    return reset_all(overwritten, /*reflexive*/true, removedExprs);
}

STORE_SCOPE(bool)::assign_unknown(
    const Expression *t,
    vector<const Expression *> *removedExprs)
{
    return remove_for_reassign(t, removedExprs);
}

STORE_SCOPE(bool)::unscope(
    const E_variable *t,
    vector<const Expression *> *removedExprs)
{
    return assign_unknown(t, removedExprs);
}

STORE_SCOPE(bool)::handle_assign(
    const Expression *t,
    int equiv,
    int equiv2,
    vector<const Expression *> *removedExprs)
{
    // Handle the t = t case
    // However, if both t and t2 are not found, we still need to erase
    // fields
    if (equiv == equiv2 && equiv != -1) {
        return false;
    }

    if(equiv2 != -1) {
        // Increase reference count before attempting to remove.
        ++valueof.get_existing(equiv2).refCount;
    }

    remove_for_reassign(t, removedExprs);
    if(equiv2 != -1) {
        LET(p, equiv_class.insert(make_pair(t, equiv2)));
        // The "remove_for_reassign" above should make sure that we're
        // actually adding.
        cond_assert(p.second);
    }
    return true;
}

STORE_SCOPE(void)::merge_equiv(int eq1, int eq2) {
    if(eq1 == eq2) {
        return;
    }
    valueof.get_existing(eq2).refCount += valueof.get_existing(eq1).refCount;
    valueof.erase(eq1);
    foreach(i, equiv_class) {
        if(i->second == eq1) {
            i->second = eq2;
        }
    }
}

STORE_SCOPE(void)::merge_equiv(const Expression *t1, const Expression *t2) {
    LET(i1, equiv_class.find(t1));
    LET(i2, equiv_class.find(t2));
    if(i1 == equiv_class.end()) {
        if(i2 == equiv_class.end()) {
            return;
        } else {
            // Add t1 to t2's equivalence class.
            equiv_class.insert(make_pair(t1, i2->second));
            ++valueof.get_existing(i2->second).refCount;
            return;
        }
    }
    if(i2 == equiv_class.end()) {
        equiv_class.insert(make_pair(t2, i1->second));
        ++valueof.get_existing(i1->second).refCount;
        return;
    }

    merge_equiv(i1->second, i2->second);
}

STORE_SCOPE_RET_SCOPED(iterator)::find(const Expression *t) {
    int_store_t::iterator p = equiv_class.find(t);
    if(p == equiv_class.end()) {
        return end();
    }
    return iterator(p, this);
}

STORE_SCOPE_RET_SCOPED(const_iterator)::find(const Expression *t) const {
    int_store_t::const_iterator p = equiv_class.find(t);
    if(p == equiv_class.end()) {
        return end();
    }
    return const_iterator(p, this);
}

// Return the equivalence class for the given expression, if any.
STORE_SCOPE(opt_uint)::find_equiv_class(const Expression *expr) const {
    int_store_t::const_iterator p = equiv_class.find(expr);
    if(p == equiv_class.end()) {
        return opt_uint();
    }
    return p->second;
}

STORE_SCOPE(bool)::contains(const Expression *t) const {
    return ::contains(equiv_class, t) ;
}

STORE_SCOPE_RET_SCOPED(iterator)::begin() { return iterator(equiv_class.begin(), this); }
STORE_SCOPE_RET_SCOPED(iterator)::end() { return iterator(equiv_class.end(), this); }
STORE_SCOPE_RET_SCOPED(const_iterator)::begin() const { return const_iterator(equiv_class.begin(), this); }
STORE_SCOPE_RET_SCOPED(const_iterator)::end() const { return const_iterator(equiv_class.end(), this); }

STORE_SCOPE_RET_SCOPED(value_iterator)::value_begin()
{
    return value_iterator(valueof.begin());
}
STORE_SCOPE_RET_SCOPED(value_iterator)::value_end()
{
    return value_iterator(valueof.end());
}
STORE_SCOPE_RET_SCOPED(const_value_iterator)::value_begin() const
{
    return const_value_iterator(valueof.begin());
}
STORE_SCOPE_RET_SCOPED(const_value_iterator)::value_end() const
{
    return const_value_iterator(valueof.end());
}

STORE_SCOPE(iterator_pair_wrapper_t<typename STORE_CLASS::value_iterator>)::values()
{
    return wrap_iterators(value_begin(), value_end());
}
STORE_SCOPE(iterator_pair_wrapper_t<typename STORE_CLASS::const_value_iterator>)::values() const
{
    return wrap_iterators(value_begin(), value_end());
}

STORE_SCOPE(bool)::empty() const {
    return valueof.empty();
}

STORE_SCOPE(T&)::operator[](const Expression *t) {
    int_store_t::iterator p = equiv_class.find(t);
    if(p == equiv_class.end()) {
        // This is slightly subtle: rather than declaring an
        // object of type T, we pass a temporary built as "T()",
        // which means it will be "value initialized" (5.2.3p2,
        // 8.5p5).  That means when T is int, it will be 0, rather
        // than uninitialized.
        int nequiv = valueof.first_unmapped_key();
        equiv_class.insert(make_pair(t, nequiv));
        return valueof.insert(nequiv, EquivClass(T())).first.value().value;
    }
    return existing_valueof_ref(p->second);
}

STORE_SCOPE(void)::get_equivs (const Expression *e,
        vector<const Expression *> &equivs) const {
    int ec = get_equiv(e);
    if (ec == -1) return;

    int total = valueof.find(ec)->refCount - 1;
    int count = 0;
    foreach(j, equiv_class) {
        if(j->second == ec && !same_expr_memory(e, j->first)) {
            equivs.push_back(j->first);
            count++;
        }
        if (count == total) break;
    }
}


STORE_SCOPE(int)::get_equiv(const Expression *t) const {
    int_store_t::const_iterator p = equiv_class.find(t);
    if(p == equiv_class.end()) {
        return -1;
    }
    return p->second;
}

STORE_SCOPE(bool)::is_equiv(const Expression *t1,const Expression *t2) const {
    if (expr_memory_equal(t1,t2))
        return true;

    int ec1 = get_equiv(t1);
    int ec2 = get_equiv(t2);
    if (ec1>=0 && ec2>=0)
        return ec2==ec2;

    return false;
}


STORE_SCOPE(const Expression *)::get_tree_from_equiv(int equiv) const {
    if(!valueof.is_mapped(equiv)) {
        return 0;
    }
    foreach(j, equiv_class) {
        if(j->second == equiv) {
            return j->first;
        }
    }
    return 0;
}

STORE_SCOPE(size_t)::equiv_class_count(int equiv_class_num) const {
    size_t total = 0;
    foreach(i, equiv_class) {
        if(i->second == equiv_class_num)
            ++total;
    }
    return total;
}

STORE_SCOPE(void)::set_equiv(const Expression *t, int e) {
    if(e == -1) {
        reset(t);
    } else {
        LET(p, equiv_class.insert(make_pair(t, e)));
        ++valueof.get_existing(e).refCount;
        if(!p.second) {
            // Changing existing mapping. Decrease previous ref count.
            dec_ref_count(p.first->second);
            p.first->second = e;
        }
    }
}

STORE_SCOPE(void)::reset_insert(const Expression *t,
                                const T& state) {
    int_store_t::iterator equiv_it = equiv_class.find(t);
    if (equiv_it != equiv_class.end()) {
        dec_ref_count(equiv_it->second);
        int nequiv = valueof.first_unmapped_key();
        equiv_it->second = nequiv;
        valueof.insert(nequiv, EquivClass(state));
    } else {
        insert(t, state);
    }
}

STORE_SCOPE(void)::reset_all_insert(const Expression *t, const T& state, arena_t *ar) {
    Copy cp;
    foreach (equiv_it, equiv_class) {
        if (/*current_fn_trav->*/contains_expr_no_fn(equiv_it->first, t)) {
            dec_ref_count(equiv_it->second);
            int nequiv = valueof.first_unmapped_key();
            equiv_it->second = nequiv;
            valueof.insert(nequiv,  EquivClass(cp(state, ar)));
        }
    }
    insert(t, state);
}


STORE_SCOPE(template<typename pat_type> void)::get_equiv_trees(vector<Expression *>& trees,
                                                               pat_type &pat,
                                                               int e) const
{
    foreach (equiv_it, equiv_class) {
        const Expression *var = (*equiv_it).first;
        if (e == (*equiv_it).second &&
            pat.match(var)) {
            trees.push_back(const_cast<Expression *>(var));
        }
    }
}

STORE_SCOPE(void)::clear() {
    valueof.clear();
    equiv_class.clear();
}

STORE_SCOPE_RET_SCOPED(equiv_map const &)::get_valueof_map() const
{ return valueof; }

STORE_SCOPE(const typename STORE_CLASS::EquivClass &)::existing_equiv_class_ref(int equiv_class) const {
    const EquivClass *t = valueof.find(equiv_class);
    cond_assert(t != 0);
    return *t;
}

STORE_SCOPE(T &)::existing_valueof_ref(int equiv_class) {
    const STORE_CLASS *cst = this;
    return const_cast<T &>(cst->existing_valueof_ref(equiv_class));
}

STORE_SCOPE(const T &)::existing_valueof_ref(int equiv_class) const {
    return existing_equiv_class_ref(equiv_class).value;
}

#undef STORE_SCOPE
#undef STORE_CLASS
#undef ITER_SCOPE
#undef ITER_CLASS
#undef VALUE_ITER_SCOPE
#undef VALUE_ITER_CLASS

#endif
