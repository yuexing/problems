// compose_ncache.c
//
// Create a new cache that is the composition of 'n' caches.  To check for
// a cache hit, make sure that we have a cache hit in each consituent
// cache.  To update the cache, update each constituent cache.
//


//
// 
//

#include "compose-ncache.hpp"
#include "sm/state/compose-nstate.hpp"

// compose_ncache_t::compose_ncache_t
//
// Construct a composed cache from a list of individual caches.
//
compose_ncache_t::compose_ncache_t(mc_arena a,
                                   cache_t** clist,
                                   unsigned int ncaches) :
    carr(arenaVectorCopy(wrap_array(clist, ncaches), *a))
{
}

// compose_ncache_t::check_cache_hit
//
// Check a cache hit by and'ing together all the results of
// check_cache_hit for each individual cache.
//
bool compose_ncache_t::check_cache_hit
(state_t* state,
 abstract_interp_t &cur_traversal,
 const cfg_edge_t *edge,
 bool widen,
 bool first_time)
{
    compose_nstate_t* cst = static_cast<compose_nstate_t*>(state);
    bool rv = true;
    for (unsigned int i = 0;
         i < carr.size();
         i++) {
        state_t* st = cst->get_nth_state(i);
        cache_t* c = carr[i];

        // We must iterate over all caches, even if we detect a miss
        // before finishing, in order to allow them all to be updated
        // with the current state (and widen, if needed).
        if (!c->check_cache_hit_wrapper(st,
                                cur_traversal,
                                edge,
                                widen,
                                first_time)) {
            rv = false;
        }
    }
    return rv;
}

// compose_ncache_t::get_cache_component
//
// Get the 'i'th component of the cache.
//
cache_t* compose_ncache_t::get_cache_component(unsigned int i) const
{
    return carr[i];
}
