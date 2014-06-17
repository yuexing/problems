// compose_nstate.c
//
// Implementation of the composition of 'n' state machines.  The only work
// here is in the constructor where we just create a list of state
// machine states on the arena that is given.
//
// Written By: Seth Hallem
// Date: May 29, 2003
//
// (c) 2003-2013 Coverity, Inc. All rights reserved worldwide.
//

#include "containers/container-utils.hpp" // wrap_array
#include "fun-utils.hpp" // deep_call_1

#include "compose-nstate.hpp"
#include "caching/compose-ncache.hpp"
#include "sm/compose-nsm.hpp"
#include "text/stream-utils.hpp" // spaces

#include "exceptions/assert.hpp" // ostr_assert

// compose_nstate_t::compose_nstate_t
//
// Create a list of states on the given arena.
//
compose_nstate_t::compose_nstate_t(
    mc_arena a,
    sm_t *sm,
    state_t** state_list,
    unsigned int nstates) :
    state_t(a, sm),
    state_list(arenaVectorCopy(wrap_array(state_list, nstates), *a))
{
}

compose_nstate_t::compose_nstate_t(
    mc_arena a,
    sm_t *sm,
    const VectorBase<state_t *> &states) :
    state_t(a, sm),
    state_list(arenaVectorCopy(states, *a))
{
}

// compose_nstate_t::compose_nstate_t
//
// Create a compose of 2 states
//
compose_nstate_t::compose_nstate_t(mc_arena a, sm_t *sm,
                                   state_t *state1,
                                   state_t *state2)
    : state_t(a, sm),
      state_list(arenaVectorAlloc<state_t *>(2, *a))
{
    this->state_list[0] = state1;
    this->state_list[1] = state2;
}

// compose_nstate_t::compose_nstate_t
//
// Create a compose of 3 states
//
compose_nstate_t::compose_nstate_t(mc_arena a, sm_t *sm,
                                   state_t *state1,
                                   state_t *state2,
                                   state_t *state3)
    : state_t(a, sm),
      state_list(arenaVectorAlloc<state_t *>(3, *a))
{
    this->state_list[0] = state1;
    this->state_list[1] = state2;
    this->state_list[2] = state3;
}

// compose_nstate_t::compose_nstate_t
//
// Create a compose of 4 states
//
compose_nstate_t::compose_nstate_t(mc_arena a, sm_t *sm,
                                   state_t *state1,
                                   state_t *state2,
                                   state_t *state3,
                                   state_t *state4)
    : state_t(a, sm),
      state_list(arenaVectorAlloc<state_t *>(4, *a))
{
    this->state_list[0] = state1;
    this->state_list[1] = state2;
    this->state_list[2] = state3;
    this->state_list[3] = state4;
}

// compose_nstate_t::compose_nstate_t
//
// Create a compose of 5 states
//
compose_nstate_t::compose_nstate_t(mc_arena a, sm_t *sm,
                                   state_t *state1,
                                   state_t *state2,
                                   state_t *state3,
                                   state_t *state4,
                                   state_t *state5)
    : state_t(a, sm),
      state_list(arenaVectorAlloc<state_t *>(5, *a))
{
    this->state_list[0] = state1;
    this->state_list[1] = state2;
    this->state_list[2] = state3;
    this->state_list[3] = state4;
    this->state_list[4] = state5;
}

compose_nstate_t::compose_nstate_t(
    arena_t *ar,
    const compose_nstate_t &other)
    : state_t(ar, other.sm),
      state_list(arenaVectorCopy(other.state_list, *ar))
{
    foreach(i, state_list) {
        *i = (*i)->clone(ar);
    }
}

// compose_nstate_t::clone
//
// Clone a composed state.
//
state_t* compose_nstate_t::clone(mc_arena a) const
{
    return new (*a) compose_nstate_t(a, *this);
}

void compose_nstate_t::on_fn_eop(abstract_interp_t &cur_traversal)
{
    foreach(i, state_list) {
        (*i)->on_fn_eop(cur_traversal);
    }
}


// compose_nstate_t::create_empty_cache
//
// Create an empty cache.  A cache is a compose_ncache_t structure
// that is basically a holder for 'n' caches.  Each cache corresponds
// to the appropriate one of the 'n' state machines.
//
cache_t* compose_nstate_t::create_empty_cache(mc_arena a) const
{
    cache_t* carr[state_list.size()];
    for (unsigned int i = 0; i < state_list.size(); i++) {
        carr[i] = state_list[i]->create_empty_cache(a);
    }
    compose_ncache_t* c = new (a) compose_ncache_t(
        a,
        carr,
        state_list.size());
    return c;
}

// compose_nstate_t::get_state_component
//
// Get the state at position 'i'.
//
state_t* compose_nstate_t::get_nth_state(unsigned int i) const
{
    ostr_assert(i < state_list.size(),
                "Trying to get a state at position '" << i
                << "', which is out of bounds.");
  
    return state_list[i];
}

state_t* compose_nstate_t::get_sm_state(sm_t *sm) const
{
    foreach(i, state_list) {
        state_t *st = *i;
        if(st->sm == sm) {
            return st;
        }
    }
    xfailureb(
        "Trying to get a state for SM "
        << sm->get_sm_name()
        << " but it wasn't in the sub-states.");
}

void compose_nstate_t::set_nth_state(unsigned int i, state_t *s)
{
    ostr_assert(i < state_list.size(),
                "Trying to set a state at position '" << i
                << "', which is out of bounds.");

    state_list[i] = s;
}

void compose_nstate_t::set_sm_state(sm_t *sm, state_t *s)
{
    foreach(i, state_list) {
        state_t *st = *i;
        if(st->sm == sm) {
            *i = s;
            return;
        }
    }
    xfailureb(
        "Trying to set a state for SM "
        << sm->get_sm_name()
        << " but it wasn't in the sub-states.");
}

// compose_nstate_t::write_as_text
//
// Print debugging information for each component SM to 'out.'
//
void compose_nstate_t::write_as_text(StateWriter& out) const
{
    SW_WRITE_BLOCK(out,
                   stringb("[COMPOSED STATE] (" << sm->get_sm_name() << ")"),
                   "[END]");
    for (unsigned int i = 0;
         i < state_list.size();
         i++) {
        state_t* st = state_list[i];
        SW_WRITE_BLOCK(out,
                       stringb("[COMPONENT " << i << "] "
                               "(" << st->sm->get_sm_name() << ")"));
        st->write_as_text(out);
    }
}
