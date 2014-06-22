// empty_cache.c
//
// An empty cache that does nothing.  The reason for implementing the
// empty cache is that it allows us to never have NULL caches so we don't
// have to check for NULL all over the place.  Instead we make the empty
// cache perform such that its existence is irrelevant.
//

// 
//

#include "empty-cache.hpp"

// empty_cache_t::check_cache_hit
//
// Always return 'true'.  The empty cache should never prevent a cache hit
// from occurring.  If the only SM you have has an empty cache, it does
// nothing.  That seems okay.
//
bool empty_cache_t::check_cache_hit
(state_t* state,
 abstract_interp_t &cur_traversal,
 const cfg_edge_t *edge,
 bool widen,
 bool first_time)
{
  return !first_time;
}
