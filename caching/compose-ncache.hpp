// compose_ncache.h
//
// Interface to the composer cache.  The composer cache is a wrapper for
// 'n' individual caches.  All it does is call to each of the individual
// caches on all events, and look for a cache hit by anding together the
// results from all of the individual caches.
//


//
// 
//

#ifndef __COMPOSE_NCACHE_H__
#define __COMPOSE_NCACHE_H__

#include "caching.hpp"
#include "arena/arena.hpp"

class compose_ncache_t : public cache_t
{
public:

    // Constructor; create a composed cache from an array of caches.
    compose_ncache_t(mc_arena a,
                     cache_t** clist,
                     unsigned int ncaches);
  
    // All of the results of check cache hit for each cache are and'ed
    // together to determine if there is a cache hit in the composed cache.
    bool check_cache_hit(state_t* state,
                         abstract_interp_t &cur_traversal,
                         const cfg_edge_t *edge,
                         bool widen,
                         bool first_time);

    // Get the 'i'th component of the cache.
    cache_t* get_cache_component(unsigned int i) const;
    
    const VectorBase<cache_t *> &get_caches() const {
        return carr;
    }
    unsigned int get_num_caches() const { return carr.size(); }
  
private:
    // Array of caches.
    VectorBase<cache_t *> carr;
};

#endif // __COMPOSE_NCACHE_H__
