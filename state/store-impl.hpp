/**
 * store-impl.hpp
 *
 * Implementation of store.
 * If using a common instantiation, you don't need this header.
 *
 * (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
 **/
#ifndef STORE_IMPL_HPP
#define STORE_IMPL_HPP

#include "syn-store.hpp"
#include "analysis/ast/symbols/field.hpp" // get_owner_class()

#include "libs/containers/map.hpp"

/**
 * Used to statically detemine whether to call the merging algorithm
 * or not
 **/
template<typename Widen, typename State>
inline void call_widen_internal(State *state,
                                const subset_cache_pair_set_t &cache,
                                Widen w) {
    state->widen_internal(cache);
}

template<typename Widen>
inline bool is_not_null_widen(Widen w) {
    return true;
}

// Partial specialization (overload) for null_widen: do nothing
template<typename T, typename State>
inline void call_widen_internal(State *state,
                                const subset_cache_pair_set_t &cache,
                                null_widen<T> widen) {
    // Do nothing, to save time
}

template<typename T>
inline bool is_not_null_widen(null_widen<T>) {
    return false;
}

// Macros to make it easier to define templates out-of-line

// Type, with template parameters filled in.
// In particular this hides the commads from the preprocessor
#define STORE_CLASS store_t<T, MakeValueUnique, W, Cache, Copy>

// Helper to declare a function: STORE_SCOPE(<return
// type)::function_name(parameters)
#define STORE_SCOPE(type)                       \
    template<class T,                           \
        class MakeValueUnique,                  \
        class W,                                \
        class Cache,                            \
        class Copy>                             \
    type STORE_CLASS

// Helper to declare a function whose return type is in the class's scope.
#define STORE_SCOPE_RET_SCOPED(type)            \
    STORE_SCOPE(typename STORE_CLASS::type)

STORE_SCOPE()::store_t(mc_arena a, sm_t *sm) :
              subset_state_t(a, sm),
              store(a),
              uniq(MakeValueUnique::get_instance()) {
    // This will be shallow-copied when cloning state, so it's OK.
        
}

// Copy a store onto another arena.
STORE_SCOPE()::store_t(mc_arena a, const this_store_t &o)
              : subset_state_t(a, o.sm),
              store(a),
              uniq(o.uniq)
{
    Copy cp;
    foreach(i, o.store) {
        store.insert(make_pair(i->first,
                               cp(i->second, a)));
    }
}

STORE_SCOPE(cache_t *)::create_empty_cache(mc_arena a) const {
    return new (a) Cache(a);
}

STORE_SCOPE(void)::insert(const Expression *t, const T& state, bool use_tree) {
    if(use_tree) {
        store.erase(t);
    }
    // Do not use map::operator[] so that T doesn't need a
    // default ctor.
    LET(p, store.insert(make_pair(t, state)));
    if(!p.second) {
        p.first->second = state;
    }
}

#define INSERT_PAIR pair<typename STORE_CLASS::iterator, bool>

STORE_SCOPE(INSERT_PAIR)::insert(const pair<const Expression *, T> &p) {
    return store.insert(p);
}

#undef INSERT_PAIR

STORE_SCOPE(size_t)::erase(const Expression *t) {
    return store.erase(t);
}

STORE_SCOPE(void)::erase(iterator i) {
    store.erase(i);
}
STORE_SCOPE(size_t)::reset(const Expression *t) {
    return erase(t);
}
STORE_SCOPE(void)::reset(iterator i) {
    erase(i);
}

STORE_SCOPE(template<typename pred> void)::erase_all_of(pred p) {
    iterator i = begin();
    iterator endi = end();
    while(i != endi) {
        if(p(i)) {
            erase(i++);
        } else {
            ++i;
        }
    }
}

STORE_SCOPE(template<typename pred> void)::reset_all_of(pred p) {
    erase_all_of(p);
}

STORE_SCOPE(void)::erase_all_fields(const Expression *t) {
    iterator i = begin();
    iterator endi = end();
    iterator ti = find(t);
    while(i != endi) {
        if(/*current_fn_trav->*/contains_expr_no_fn(i->first, t) &&
           i != ti) {
            erase(i++);
        } else {
            ++i;
        }
    }
}

STORE_SCOPE(void)::erase_all(const Expression *t) {
    const type_t *type = t->type;
    if(type->ispointer_type_t() || type->isclass_type_t()) {
        erase_all_fields(t);
    }
    erase(t);
}

STORE_SCOPE(void)::reset_all_fields(const Expression *t) {
    erase_all_fields(t);
}

// Same as erase_all, to allow syntax compatibility with syn_store.
STORE_SCOPE(void)::reset_all(const Expression *t) {
    erase_all(t);
}

STORE_SCOPE(void)::assign_unknown(const Expression *t)
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
    erase_all(overwritten);
}

STORE_SCOPE(void)::unscope(const E_variable *var)
{
    erase_all(var);
}

STORE_SCOPE(void)::erase_pattern(AST_patterns::ExpressionPattern &pat) {
    erase_all_of(key_match(pattern_fun(pat)));
}

STORE_SCOPE(void)::reset_pattern(AST_patterns::ExpressionPattern &pat) {
    erase_pattern(pat);
}

STORE_SCOPE(void)::set_all(const Expression *t, const T& val) {
    for(iterator i = begin(); i != end(); ++i) {
        if(/*current_fn_trav->*/contains_expr_no_fn(i->first, t)) {
            i->second = val;
        }
    }
}

STORE_SCOPE(void)::set_all_insert(const Expression *t, const T& val, bool use_tree) {
    set_all(t, val);
    insert(t, val, use_tree);
}

STORE_SCOPE(void)::assign(const Expression *t, const Expression *t2) {
    iterator it = find(t2);
    erase_all(t);
    if(it != end()) {
        insert(t, it->second);
    }
}

STORE_SCOPE_RET_SCOPED(iterator)::find(const Expression *t) {
    return store.find(t);
}

STORE_SCOPE_RET_SCOPED(const_iterator)::find(const Expression *t) const {
    return store.find(t);
}

STORE_SCOPE(bool)::has(const Expression *t, const T& val) {
    iterator p = find(t);
    return p != end() && MakeValueUnique::equals(p->second, val);
}

STORE_SCOPE(bool)::contains(const Expression *t) const {
    return ::contains(store, t) ;
}

STORE_SCOPE(void)::clear() {
    store.clear();
}

STORE_SCOPE(void)::compute_hash(subset_hash_t& v) const {
    v.reserve(this->size());
    foreach(p, *this) {
        v.push_back(make_pair(p->first,
                              uniq->get_unique_id(p->second)));
    }
}

STORE_SCOPE(void)::print_pair(ostream &out,
                              const pair<const Expression *, size_t> &p) const {
    out << p.first << " -> " << uniq->get_from_id(p.second);
}
    
STORE_SCOPE(void)::print_cache(IndentWriter &ostr, const subset_cache_pair_set_t &s) const {
    if(s.empty()) {
        ostr << "[]";
        return;
    }
    ostr << "[\n";
    ostr.indent();
    foreach(i, s) {
        print_pair(ostr, *i);
        ostr << '\n';
    }
    ostr.dedent();
    ostr << ']';
}

STORE_SCOPE(bool)::is_equal(const state_t& oth) const {
    this_store_t& other = (this_store_t&) oth;
    if(size() != other.size())
        return false;

    const_iterator p;
    for(p = begin(); p != end(); ++p) {
        const_iterator q = other.find(p->first);
        if(q == other.end())
            return false;
        if(!MakeValueUnique::equals(p->second, q->second))
            return false;
    }
    return true;
}

STORE_SCOPE(state_t *)::clone(mc_arena a) const {
    return new (a) this_store_t(a, *this);
}

STORE_SCOPE(bool)::empty() {return store.empty();}
    
STORE_SCOPE_RET_SCOPED(iterator)::begin() { return store.begin(); }
STORE_SCOPE_RET_SCOPED(iterator)::end() { return store.end(); }

STORE_SCOPE_RET_SCOPED(const_iterator)::begin() const { return store.begin(); }
STORE_SCOPE_RET_SCOPED(const_iterator)::end() const { return store.end(); }

STORE_SCOPE_RET_SCOPED(size_type)::size() const { return store.size(); }

STORE_SCOPE(T&)::operator[](const Expression *t) {
    return store[t];
}

STORE_SCOPE(string)::value_at_id_string(size_t id) const {
    ostringstream return_ss;
    return_ss << uniq->get_from_id(id);
    return return_ss.str();
}


STORE_SCOPE(void)::write_as_text(StateWriter& out) const {
    out.openStateBlock(stringb("STORE (" << sm->get_sm_name() <<  "):"));
    const_iterator p;
    for(p = begin(); p != end(); ++p) {
        out << p->first << " -> " << p->second << endl;
    }
    out.closeStateBlock("END STORE");
}

STORE_SCOPE(bool)::widens() const {
    return is_not_null_widen(value_widener);
}

STORE_SCOPE(void)::widen(const subset_cache_pair_set_t &cache) {
    call_widen_internal(this, cache, value_widener);
}
    
STORE_SCOPE(void)::widen_internal(const subset_cache_pair_set_t &cache)
{
    if(cache.empty()) {
        return;
    }
    // Iterate over the state and look up in the cache, as state is
    // usually smaller.
    for(LET(it, store.begin()); it != store.end(); /**/) {
        LET(p, valuesWithExpr(cache, it->first));
        for(; p.first != p.second; ++p.first) {
            const T &cache_val = uniq->get_from_id(p.first->second);
            T &oldval = it->second;
            if(!value_widener(this, it->first, oldval, cache_val)) {
                store.erase(it++);
                goto next_in_state;
            }
        }
        ++it;
    next_in_state:;
    }
}

STORE_SCOPE_RET_SCOPED(store_map const &)::get_private_store_map() const { return store; }

#undef STORE_SCOPE
#undef STORE_SCOPE_RET_SCOPED
#undef STORE_CLASS

#endif  // STORE_IMPL_HPP
