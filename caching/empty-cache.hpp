// empty_cache.h
//
// Interface to a completely empty cache that does nothing.  It is only
// used as a dummy instead of returning NULL for those state machines that
// do not have caching but instead rely on the caching of the SMs with
// which they are composed.
//

// 
//

#ifndef __EMPTY_CACHE_H__
#define __EMPTY_CACHE_H__

#include "caching/caching.hpp"
#include "libs/arena/arena-fwd.hpp" // arena_t

class empty_cache_t : public cache_t
{
public:
    empty_cache_t() { }
    empty_cache_t(arena_t *ar) { }

    bool check_cache_hit
    (state_t* state,
     abstract_interp_t &cur_traversal,
     const cfg_edge_t *edge,
     bool widen,
     bool first_time);
};

#endif // __EMPTY_CACHE_H__
