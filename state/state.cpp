// state.c
//
// Implementation of the generic state interface.  All we have here is the
// ability to get and set the dirty bit.
//
// Written By: Seth Hallem
// Date: May 27, 2003
//
// (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
//

#include "text/stream-utils.hpp"
#include "arena/arena.hpp"

#include "sm/state/state.hpp"
#include "caching/eqcache.hpp"

// state_t::state_t
//
// construct a state_t
//
state_t::state_t(mc_arena a, sm_t *sm) :
  sm(sm),
  arena(a)
{
}

// state_t::get_arena
//
// Get the arena for this state
//
mc_arena state_t::get_arena() const
{
    return this->arena;
}

// SGM: I do not understand why we would ever do this.  See my
// comments at the definition of scoped_ptr<state_t> in state.hpp.
void state_t::free_arena()
{                        
    // Nullify 'arena' to protect against subsequent use.
    //
    // We have to do this before freeing it, because 'this' object and
    // hence the pointer lives on the arena itself!  (So, the arena
    // freeing code should also overwrite it, but I want to be sure.)
    arena_t *tmp = arena;
    arena = NULL;
    
    // Now it is safe to free the arena.
    delete static_cast<heap_allocated_arena_t *>(tmp);
}

bool state_t::is_empty_state() const
{
    return false;
}

DEFN_AST_DOWNCASTS(state_t, subset_state_t);

// state_t::debug_print
//
// Print out for debugging purposes.
//
void state_t::debug_print(void) const
{
    StateWriter writer(cout);
    write_as_text(writer);
}

void state_t::on_fn_eop(abstract_interp_t &cur_traversal)
{
    // Do nothing by default.
}

subset_state_t::subset_state_t(mc_arena a, sm_t *sm) : state_t(a, sm) {
}

bool subset_state_t::issubset_state_t() const
{
    return true;
}

static subset_cache_pair_set_t &toSet(
    const VectorBase<pair<const Expression *, size_t> > &cache,
    arena_t &ar)
{
    subset_cache_pair_set_t *rv = subset_cache_pair_set_t::create(&ar);
    foreach(i, cache) {
        rv->insert(rv->end(), *i);
    }
    return *rv;
}

void subset_state_t::widen(const VectorBase<pair<const Expression *, size_t> > &cache)
{
    if(!widens()) {
        return;
    }
    stack_allocated_arena_t ar("widen convert");
    widen(toSet(cache, ar));
}

void subset_state_t::print_cache(
    IndentWriter &out,
    const VectorBase<pair<const Expression *, size_t> > &cache) const
{
    stack_allocated_arena_t ar("widen convert");
    print_cache(out, toSet(cache, ar));
}

// eq_state_t::eq_state_t
//
// Construct an eq_state_t
//
eq_state_t::eq_state_t(mc_arena a, sm_t *sm) : state_t(a, sm) {
}

cache_t *eq_state_t::create_empty_cache( mc_arena a ) const
{
    return new (a) eq_cache_t(a);
}

// eq_state_t::widen
//
// By default, do no widening.
//
void eq_state_t::widen(const eq_state_t* other) {
}


ostream &operator<<(ostream &out, const state_t &s) {
    StateWriter iw(out);
    s.write_as_text(iw);
    return out;
}

void gdb_print_state(const state_t *state) {
    state->debug_print();
}
