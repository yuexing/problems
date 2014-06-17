/**
 * syn-store-impl.hpp
 *
 * Implementation of syn-store.
 * If using a common instantiation, you don't need this header.
 *
 * (c) 2012-2014 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef SYN_STORE_IMPL_HPP
#define SYN_STORE_IMPL_HPP

#include "store-impl.hpp"
#include "syn-store.hpp"

// If you want syn_store_t's impl, you also want
// primitive_syn_store_t's impl.
#include "primitive-syn-store-impl.hpp"

// See store-impl.hpp for a description of these macros
#define STORE_CLASS syn_store_t<T, MakeValueUnique, W, Cache, Copy>

#define STORE_SCOPE(type)                       \
    template<class T,                           \
        class MakeValueUnique,                  \
        class W,                                \
        class Cache,                            \
        class Copy> type STORE_CLASS

#define STORE_SCOPE_RET_SCOPED(type)            \
    STORE_SCOPE(typename STORE_CLASS::type)

STORE_SCOPE()::syn_store_t(mc_arena a, sm_t *sm, W w) :
              subset_state_t(a, sm),
              primitive_store_t(a, sm),
              value_widener(w),
              uniq(MakeValueUnique::get_instance())
{
}

// Copy an existing syn_store_t onto another arena.
STORE_SCOPE()::syn_store_t(mc_arena a, const this_store_t &o) :
              subset_state_t(a, o.sm),
              primitive_store_t(a, o),
              value_widener(o.value_widener),
              uniq(o.uniq)
{
}
    
STORE_SCOPE(cache_t *)::create_empty_cache(mc_arena a) const {
    return new (a) Cache(a);
}

STORE_SCOPE(bool)::has(const Expression *t, const T& val) const {
    const_iterator p = find(t);
    if(p == end()) {
        return false;
    }
    return MakeValueUnique::equals(p.valueRef(), val);
}

STORE_SCOPE(void)::compute_hash(subset_hash_t& v) const {
    v.reserve(this->size());
    foreach(p, *this) {
        v.push_back(make_pair(p.key(),
                           uniq->get_unique_id(p.valueRef())));
    }
}

STORE_SCOPE(void)::print_pair(ostream &out,
                              const pair<const Expression *, size_t> &p) const {
    PRINT_AST_NOBLOCKS(p.first, out);
    out << " -> " << uniq->get_from_id(p.second);
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
    const this_store_t& other =
        static_cast<const this_store_t&>(oth);
    if(equiv_class.size() != other.equiv_class.size())
        return false;

    const_iterator p;
    for(p = begin(); p != end(); ++p) {
        const_iterator q = other.find(p.key());
        if(q == other.end())
            return false;
        if(!MakeValueUnique::equals(p.valueRef(), q.valueRef()))
            return false;
    }
    return true;
}

STORE_SCOPE(state_t *)::clone(mc_arena a) const {
    return new (a) this_store_t(a, *this);
}

STORE_SCOPE(void)::set_all(const Expression *t, const T& val) {
    this->primitive_store_t::set_all(t, val, this->get_arena());
}

STORE_SCOPE(void)::set_all_insert(const Expression *t, const T& val) {
    this->primitive_store_t::set_all_insert(t, val, this->get_arena());
}

STORE_SCOPE(void)::reset_all_insert(const Expression *t, const T& val, arena_t *ar) {
    this->primitive_store_t::reset_all_insert(t, val, this->get_arena());
}

STORE_SCOPE(string)::value_at_id_string(size_t id) const {
    ostringstream return_ss;
    return_ss << uniq->get_from_id(id);
    return return_ss.str();
}

STORE_SCOPE(void)::write_as_text(StateWriter& out) const {
    out.openStateBlock(stringb("SYN_STORE (" << sm->get_sm_name() << "):"));
    foreach(q, *this) {
        PRINT_AST_NOBLOCKS(q.key(), out);
        out << " -> " << q.valueRef()
            << " (equiv_class=" << q.equiv_class() << ")"
            << endl;
    }
    out.closeStateBlock("END SYN_STORE");
}

STORE_SCOPE(bool)::widens() const {
    return is_not_null_widen(value_widener);
}

STORE_SCOPE(void)::widen(const subset_cache_pair_set_t &s) {
    call_widen_internal(this, s, value_widener);
}
    
STORE_SCOPE(bool)::if_widened_callback(const iterator &i, void *arg)
{
    IfWidened &w = *static_cast<IfWidened *>(arg);
    return w.store->if_widened(i, w.cache);
}

STORE_SCOPE(bool)::if_widened(const iterator &i, const subset_cache_pair_set_t &cache)
{
    LET(p, valuesWithExpr(cache, i.key()));
    for(; p.first != p.second; ++p.first) {
        const T &cache_val = uniq->get_from_id(p.first->second);
        T &oldval = i.valueRef();
        if(!value_widener(this, i.key(), oldval, cache_val)) {
            return true;
        }
    }
    return false;
}

STORE_SCOPE(void)::widen_internal(const subset_cache_pair_set_t &cache)
{
    if(cache.empty()) {
        return;
    }
    // Iterate over the state and look up in the cache, as state is
    // usually smaller.
    IfWidened w(cache, this);
    this->erase_if(if_widened_callback, &w);
}

#undef STORE_SCOPE
#undef STORE_SCOPE_RET_SCOPED
#undef STORE_CLASS

#endif // SYN_STORE_IMPL_HPP
