// caching.c
//
// Stub implementation of the virtual base class for all caches.
//


//
// 
//

#include "caching.hpp"

#include "analysis/sm/sm.hpp"
#include "analysis/sm/state/state.hpp"

#include "time/activity-path-timer.hpp"          // START_STOP_ACTIVITY

namespace debug {
    bool caching = false;
}

bool cache_t::check_cache_hit_wrapper
    (state_t* state,
     abstract_interp_t &cur_traversal,
     const cfg_edge_t *edge,
     bool widen,
     bool first_time) {
    START_STOP_ACTIVITY(state->sm->cache_timer());
    return check_cache_hit(state,
                           cur_traversal,
                           edge,
                           widen,
                           first_time);
}
