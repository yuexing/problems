// eqcache.c
//
// An equality cache.
//


//
// 
//

#include "eqcache.hpp"
#include "sm/sm.hpp"
#include "sm/state/state.hpp"
#include "traversal/abstract-interp.hpp" // fn arena
 
#include "time/activity-path-timer.hpp"

eq_cache_t::eq_cache_t(arena_t *ar):
    ht(ar) {
}

// eq_cache_t::check_cache_hit
//
// Check for a hit in the subset cache.  Do so by converting the state
// machine state into a set of 'size_t's, then determining if that set of
// 'size_t's is a subset of the 'size_t's already in the cache.
//
bool eq_cache_t::check_cache_hit(state_t* sm_st,
                                 abstract_interp_t &cur_traversal,
                                 const cfg_edge_t *edge,
                                 bool widen,
                                 bool first_time)
{
    bool result = false;
    sm_t *sm = sm_st->sm;
    START_STOP_ACTIVITY(sm->cache_timer());
    // Add a sub-timer with constant name for later aggregation
    DEF_FREQUENT_ACTIVITY("Equality Caching");

    eq_state_t *state = (eq_state_t*)sm_st;

    if(debug::caching) {
        StateWriter writer(cout);
        writer << endl << "Cache for: (" <<
            sm->get_sm_name() << ")" << endl;
        state->write_as_text(writer);
    }
    
    // Check for membership.
    if (contains(ht,state)) {
        result = true;
    } else {
        eq_state_t* dup_sm_st =
            (eq_state_t*) sm->clone_state(sm_st, cur_traversal.get_cache_arena());
        ht.insert(dup_sm_st);
    }

    // widen 'sm_st' against all of the states already in the store
    if(widen) {
        foreach(st_it, ht) {
            state->widen(*st_it);
        }
    }

    if (debug::caching) {
        cout << "result: " << (result ? "HIT" : "MISS") << endl;
    }
    return result;
}

// eq_cache_hash_t::operator()
//
// Hashing function for the equality cache.  This function wraps up the
// functionality of converting the SM to a set of integers, then hashing
// the entire set..
//
size_t eq_cache_hash_t::operator()(const state_t* sm_st) const
{
    eq_state_t *state = (eq_state_t*) sm_st;
    return state->compute_hash();
}

// eq_cache_equality_t::operator()
//
// Equality comparator for the equality cache.  This function wraps up the
// functionality of structurally comparing two states using the 'is_equal'
// function.
//
bool eq_cache_equality_t::operator()(const state_t* sm_st1,
                                     const state_t* sm_st2) const
{
    eq_state_t *state1 = (eq_state_t *)sm_st1;
    eq_state_t *state2 = (eq_state_t *)sm_st2;
    return (state1->is_equal(*state2));
}
