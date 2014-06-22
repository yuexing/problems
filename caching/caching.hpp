// caching.h
//
// Generic interface to a cache.  The class 'cache_t' declared in this
// file cannot be used to create instances.  Instead it provides an
// interface and some default (empty) function bodies for those functions
// that are truly optional in the derived classes.
//


//
// 
//

#ifndef __CACHING_H__
#define __CACHING_H__

#include "ast/cfg-fwd.hpp" // cfg_edge_t
#include "sm/state/state-fwd.hpp" // state_t
#include "traversal/abstract-interp-fwd.hpp"
#include "libs/arena/arena.hpp" // AllocateOnlyOnArena

namespace debug {
    // Info on abstract interpretation caching
    extern bool caching;
}

// class cache_t
//
// Generic interface to a cache, which determines when the extension has
// reached a fixed point.
//
class cache_t: public AllocateOnlyOnArena
{
public:

    virtual ~cache_t() { }

    /**
     * Determine if the SM state 'state' was already seen in the cache.
     * The implementation of this function depends entirely on how the
     * particular cache has chosen to determine whether or not a state
     * is a member of the cache.  This function also adds states that
     * miss in the cache into the cache so that the next time around
     * there will be a cache hit.
     * If "widen" is set, the cache may choose tp perform widening to
     * increase the likelihood of a cache hit (for instance
     * inerval_fpp does widening). widening is occasionally called
     * "merging" in the Coverity terminology.
     * If "first_time" is given, this is the first time through this
     * cache. Return value will be ignored and considered "false".
     **/
    virtual bool check_cache_hit(state_t* state,
                                 abstract_interp_t &cur_traversal,
                                 const cfg_edge_t *edge,
                                 bool widen,
                                 bool first_time) = 0;

    // Wrapper that includes timing measurements (as in the "sm_t" interface)
    bool check_cache_hit_wrapper
    (state_t* state,
     abstract_interp_t &cur_traversal,
     const cfg_edge_t *edge,
     bool widen,
     bool first_time);
};

void caching_unit_tests();

#endif // ifndef __CACHING_H__
