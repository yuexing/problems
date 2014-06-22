/**
 * \file compose-eq-cache.hpp
 *
 * Cache for a composition of "subset states", using "equality"
 * caching.
 *
 * 
**/
#ifndef COMPOSE_SUBSET_CACHE_HPP
#define COMPOSE_SUBSET_CACHE_HPP

#include "caching.hpp" // cache_t

#include "analysis/sm/state/state.hpp" // subset_state_t
#include "analysis/sm/state/compose-nstate.hpp" // compose_nstate_t

#include "libs/containers/container-utils.hpp" // VectorBase

// The corresponding state for this should be a composed_nstate_t, and
// each state in it should be a subset_state_t or an empty_state_t.
struct compose_eq_cache_t: public cache_t
{
    compose_eq_cache_t(arena_t *ar);

    // What we'll put in cache as state (using subset_state_t::compute_hash)
    typedef VectorBase<pair<const Expression *, size_t> > CachedState;

    struct CachedStateComposition {
        CachedStateComposition(arena_t *ar,
                               const VectorBase<subset_state_t *> &subStates);
        // Copy ctor (for moving to a different arena)
        CachedStateComposition(arena_t *ar,
                               const CachedStateComposition &other);
        // Composition of states.
        VectorBase<CachedState> states;
        int compare(const CachedStateComposition &) const;
        struct ptr_lt_t {
            bool operator()(const CachedStateComposition *s1,
                            const CachedStateComposition *s2) const {
                return s1->compare(*s2) < 0;
            }
        };
        void write_as_text(IndentWriter &out,
                           const VectorBase<subset_state_t *> &subStates) const;
    };

    typedef
    arena_set_t<CachedStateComposition *,
        CachedStateComposition::ptr_lt_t>
    CachedStateSet;
    CachedStateSet cachedStates;

    // To speed up widening, instead of iterating over all the cached
    // states (many pairs would be repeated), we'll iterate over the
    // set of pairs.
    // The size of this array is the number of states that actually
    // need widening (see subset_state_t::widens())
    // It's initialized the first time check_cache_hit is called, and
    // only if "widens" is true.
    struct WidenInfo: public AllocateOnlyOnArena {
        WidenInfo(const vector<subset_state_t *> &subStates,
                  const CachedStateSet &cachedStates);
        void widen(const vector<subset_state_t *> &subStates);
        void add(const CachedStateComposition &newState);
        // Index of the states that do widenin.
        VectorBase<int> statesThatWiden;
        VectorBase<subset_cache_pair_set_t *> widenCaches;
    };
    WidenInfo *widenInfo;

    override bool check_cache_hit(
        state_t *state,
        abstract_interp_t &cur_traversal,
        const cfg_edge_t *edge,
        bool widen,
        bool first_time);
};

// A composition of subset states, that will use equality caching.
struct compose_eq_state_t: public compose_nstate_t {
    compose_eq_state_t(
        arena_t *ar,
        sm_t *sm,
        const VectorBase<state_t *> &states);
    // Ctor for "clone".
    compose_eq_state_t(
        arena_t *ar,
        const compose_eq_state_t &other);

    state_t *clone(arena_t *ar) const;
    override cache_t *create_empty_cache(arena_t *ar) const;
};

#endif // COMPOSE_SUBSET_CACHE_HPP
