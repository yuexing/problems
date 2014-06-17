// (c) 2004-2012 Coverity, Inc. All rights reserved worldwide.
#include "empty-state.hpp"
#include "text/stream-utils.hpp" // spaces

// Create an empty cache
cache_t *empty_state_t::create_empty_cache(mc_arena a) const {
    return new (a) empty_cache_t;
}

state_t* empty_state_t::clone(mc_arena a) const
{
    // Need to clone, since states are deleted at end of every path.
    return new (a) empty_state_t(a, sm);
}

void empty_state_t::write_as_text(StateWriter &out) const {
    out << "<empty state>" << endl;
}

bool empty_state_t::is_empty_state() const
{
    return true;
}
