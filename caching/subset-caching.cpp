// ast-subset-caching.cpp
//
// Implementation of a simple caching scheme.  In this scheme, a state is
// hashed to a set of numbers.  Those numbers are then compared for subset
// membership in the cache.  If the state is a subset of the cache, we
// have a cache hit.  If not, we have a cache miss.  No edges are tracked
// here, so this only works well for local analyses.  All local Metal
// state machines use this scheme.
//


//
// 
//

// This dir
#include "subset-caching.hpp"

// Analysis
#include "sm/sm.hpp"
#include "sm/state/state.hpp"
#include "ast/ast/cfg.hpp" // cfg_edge_t
#include "traversal/abstract-interp.hpp" // abstract_interp_t

#include "text/printed-counter.hpp"
#include "generic-subset-caching-impl.hpp"

pair<subset_cache_pair_set_t::const_iterator,
    subset_cache_pair_set_t::const_iterator>
valuesWithExpr(const subset_cache_pair_set_t &cache, const Expression *expr)
{
    return make_pair(cache.lower_bound(make_pair(expr, (size_t)0)),
                     cache.upper_bound(make_pair(expr, (size_t)-1)));
}

unsigned int equality_caching_threshold_default = 0;
unsigned int slice_equality_caching_threshold_default = 81 /* 3*3*3*3 (4 nested loops) */;
mc_arena subset_cache_arena;

// subset_cache_t::check_cache_hit
//
// Check for a hit in the subset cache.  Do so by converting the state
// machine state into a set of 'size_t's, then determining if that set of
// 'size_t's is a subset of the 'size_t's already in the cache.
//
bool subset_cache_base_t::check_cache_hit
(state_t* state,
 abstract_interp_t &cur_traversal,
 const cfg_edge_t *edge,
 bool widen,
 bool first_time)
{
    const basic_block_t *block = edge ? edge->to : cur_traversal.current_block();

    return subset_cache_type::check_cache_hit
        <subset_state_t, subset_cache_base_t>(
            state,
            this,
            state->sm->get_slicing_expressions(block),
            widen,
            first_time,
            /*subset*/novalue_is_bottom,
            cur_traversal.get_cache_arena());
}
