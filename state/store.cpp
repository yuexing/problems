// (c) 2004,2006-2008,2010,2012-2013 Coverity, Inc. All rights reserved worldwide.
/**
 * \file store.cpp
 *
 * Explicit instantiation of "new AST" stores, for compilation testing
 *
 **/

#include "store-impl.hpp"
#include "syn-store-impl.hpp"
#include "checker-syn-store-impl.hpp"
#include "derive-syn-store-impl.hpp"
#include "source-deriver-state-impl.hpp"
#include "set-state.hpp"
#include "primitive-syn-store-impl.hpp"

template class store_t<int, int_unique_t>;
template class store_t<const char*, string_unique_t>;
template class store_t<const Expression *, expr_unique_t>;

#define INSTANTIATE_PRIMITIVE_SYN_STORE(val, copy)                      \
    template class primitive_syn_store_t<val, copy>;                    \
    template class primitive_syn_store_t<val, copy>::iter<>;            \
    template class primitive_syn_store_t<val, copy>::iter<              \
        primitive_syn_store_t<val, copy>::equiv_class_map::const_iterator, \
        const primitive_syn_store_t<val, copy>,                         \
        primitive_syn_store_t<val, copy>::equiv_map::const_iterator, val const>; \
    template class primitive_syn_store_t<val, copy>::value_iter<>;      \
    template class primitive_syn_store_t<val, copy>::value_iter<        \
        primitive_syn_store_t<val, copy>::equiv_map::const_iterator, val const>

INSTANTIATE_PRIMITIVE_SYN_STORE(int, ShallowCopy<int>);
INSTANTIATE_PRIMITIVE_SYN_STORE(const char *, ShallowCopy<const char *>);
INSTANTIATE_PRIMITIVE_SYN_STORE(const Expression *, ShallowCopy<const Expression *>);

INSTANTIATE_PRIMITIVE_SYN_STORE(err_t *, CloneCopy<err_t>);

INSTANTIATE_PRIMITIVE_SYN_STORE(model_t *, CloneCopy<model_t>);

template class syn_store_t<int, int_unique_t>;
template class syn_store_t<const char*, string_unique_t>;
template class syn_store_t<const Expression*, expr_unique_t>;
template class syn_store_t<model_t *,
    invalid_unique_t<model_t *>,
    null_widen<model_t *>,
    empty_cache_t,
    CloneCopy<model_t> >;
template class syn_store_t<err_t *,
    invalid_unique_t<err_t *>,
    null_widen<err_t *>,
    empty_cache_t,
    CloneCopy<err_t> >;

template class checker_syn_store_t<int, int_unique_t>;
template class checker_syn_store_t<const Expression *, expr_unique_t>;

template class legacy_derive_syn_store_t<int, int_unique_t>;
template class legacy_derive_syn_store_t<const Expression *, expr_unique_t>;

template class source_deriver_state_t<int, int_unique_t>;
template class source_deriver_state_t<const Expression *, expr_unique_t>;

template class set_state_t<const action_interface_t *, action_interface_t::ptr_lt_t>;


bool pattern_matches_expr(const Expression *e, void *pat)
{
    return static_cast<AST_patterns::ExpressionPattern *>(pat)->match(e);
}

bool expr_contains_expr_strict(const Expression *haystack, void *needle)
{
    return contains_expr_no_fn(
    haystack,
    static_cast<const Expression *>(needle),
    /*reflexive*/false);
}

bool expr_contains_expr_reflexive(const Expression *haystack, void *needle)
{
    return contains_expr_no_fn(
    haystack,
    static_cast<const Expression *>(needle),
    /*reflexive*/true);
}
