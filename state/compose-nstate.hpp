// compose_nstate.h
//
// Interface to the composition of 'n' state machine states.
//
// Written By: Seth Hallem
// Date: May 29, 2003
//
// (c) 2003-2013 Coverity, Inc. All rights reserved worldwide.
//

#ifndef __COMPOSE_NSTATE_H__
#define __COMPOSE_NSTATE_H__

#include "state.hpp"

class compose_nstate_t : public state_t
{
public:
    // Create a composition of 'n' states on the arena 'a'.  The states to
    // compose are passed in the list 'state_list', which contains 'nstates'
    // elements.
    compose_nstate_t(mc_arena a, sm_t *sm,
                     state_t** state_list,
                     unsigned int nstates);

    compose_nstate_t(mc_arena a, sm_t *sm,
                     const VectorBase<state_t *> &states);

    // Compose only two states.
    compose_nstate_t(mc_arena a, sm_t *sm,
                     state_t* state1,
                     state_t* state2);

    // Compose only three states.
    compose_nstate_t(mc_arena a, sm_t *sm,
                     state_t* state1,
                     state_t* state2,
                     state_t* state3);

    // Compose only four states.
    compose_nstate_t( mc_arena a, sm_t *sm,
                      state_t *state1,
                      state_t *state2,
                      state_t *state3,
                      state_t *state4 );

    // Compose only five states. Where will it end??
    compose_nstate_t( mc_arena a, sm_t *sm,
                      state_t *state1,
                      state_t *state2,
                      state_t *state3,
                      state_t *state4,
                      state_t *state5 );

    // Ctor for "clone". Will clone "other"'s states.
    compose_nstate_t(arena_t *ar, const compose_nstate_t &other);

    // Clone this composed state on the arena 'a'.  Return the clone.
    override state_t* clone(mc_arena a) const;

    // Calls on_fn_eop on all sub-states.
    override void on_fn_eop(abstract_interp_t &cur_traversal);

    // Create an empty cache.  A cache is a compose_ncache_t structure
    // that is basically a holder for 'n' caches.  Each cache
    // corresponds to the appropriate one of the 'n' state machines.
    virtual cache_t *create_empty_cache(mc_arena a) const;
  
    // Accessor to retrieve the state at position 'i'.
    state_t* get_nth_state(unsigned int i) const;
    state_t* get_nth_state_unsafe(unsigned int i) const
    {
        // Same as above, but optimized for speed, when the index has
        // alteady been checked.
        // We call this a lot.
        return state_list.data()[i];
    }

    // Return the sub-state corresponding to the given sm, which must
    // be present.
    state_t* get_sm_state(sm_t *sm) const;

    // Change the state at position 'i'.
    void set_nth_state(unsigned int i, state_t *s);

    // Change the state for the given sm, which must
    // be present.
    void set_sm_state(sm_t *sm, state_t *s);

    // Return the number of states in the composition
    unsigned get_state_num() const {
        return state_list.size();
    }

    const VectorBase<state_t *> &get_states() const {
        return state_list;
    }
    
    // Print debugging information for the composed state by printing out
    // each component.
    virtual void write_as_text(StateWriter& out) const;

protected:
    // Default constructor, for manually adding states later
    compose_nstate_t(mc_arena a, sm_t *sm) :
        state_t(a, sm),
        state_list()
    { }

    // Component states. Allocated on the state's arena.
    VectorBase<state_t *> state_list;
};

// These template classes make it easier to use compose_nstate_t.
template<class T1, class T2>
class compose_state : public compose_nstate_t {
public:
    typedef compose_state<T1, T2> this_state_t;

    compose_state(mc_arena a, sm_t *sm) 
        : compose_nstate_t(a, sm, new (a) T1(a, sm), new (a) T2(a, sm)) { }

    T1* first() const { return static_cast<T1*>(get_nth_state_unsafe(0)); }
    T2* second() const { return static_cast<T2*>(get_nth_state_unsafe(1)); }

    state_t* clone(mc_arena a) const {
        state_t *rv = new (a) this_state_t(a, this->sm, *this);
        return rv;
    }

protected:
    // Don't use this constructor except for cloning.
    compose_state(mc_arena a, sm_t *sm, const this_state_t &o)
        : compose_nstate_t(a, sm, o.first()->clone(a), o.second()->clone(a)) {
    }

    compose_state(
        arena_t *a,
        sm_t *sm,
        T1 *s1,
        T2 *s2)
        : compose_nstate_t(a, sm, s1, s2) {
    }
};

template<class T1, class T2, class T3>
class compose_state3 : public compose_nstate_t {
public:
    typedef compose_state3<T1, T2, T3> this_state_t;

    compose_state3(mc_arena a, sm_t *sm) 
        : compose_nstate_t(a, sm, new (a) T1(a, sm), new (a) T2(a, sm), new (a) T3(a, sm)) { }

    compose_state3(mc_arena a, sm_t *sm,
                   T1 *s1, T2 *s2, T3 *s3)
        : compose_nstate_t(a, sm, s1, s2, s3) { }

    T1* first() const { return static_cast<T1*>(get_nth_state_unsafe(0)); }
    T2* second() const { return static_cast<T2*>(get_nth_state_unsafe(1)); }
    T3* third() const { return static_cast<T3*>(get_nth_state_unsafe(2)); }

    state_t* clone(mc_arena a) const {
        state_t *rv = new (a) this_state_t(a, sm, *this);
        return rv;
    }

protected:
    // Don't use this constructor except for cloning.
    compose_state3(mc_arena a, sm_t *sm, const this_state_t &o)
        : compose_nstate_t(a, sm,
                           o.first()->clone(a), 
                           o.second()->clone(a),
                           o.third()->clone(a)) { 
    }
};

template<class T1, class T2, class T3, class T4>
class compose_state4 : public compose_nstate_t {
public:
    typedef compose_state4<T1, T2, T3, T4> this_state_t;

    compose_state4(mc_arena a, sm_t *sm) 
        : compose_nstate_t(a, sm, new (a) T1(a, sm), new (a) T2(a, sm), new (a) T3(a, sm), new (a) T4(a, sm)) { }

    compose_state4(mc_arena a, sm_t *sm,
                   T1 *s1, T2 *s2, T3 *s3, T4 *s4)
        : compose_nstate_t(a, sm, s1, s2, s3, s4) { }

    T1* first() const { return static_cast<T1*>(get_nth_state_unsafe(0)); }
    T2* second() const { return static_cast<T2*>(get_nth_state_unsafe(1)); }
    T3* third() const { return static_cast<T3*>(get_nth_state_unsafe(2)); }
    T4* fourth() const { return static_cast<T4*>(get_nth_state_unsafe(3)); }

    state_t* clone(mc_arena a) const {
        state_t *rv = new (a) this_state_t(a, sm, *this);
        return rv;
    }

protected:
    // Don't use this constructor except for cloning.
    compose_state4(mc_arena a, sm_t *sm, const this_state_t &o)
        : compose_nstate_t(a, sm,
                           o.first()->clone(a), 
                           o.second()->clone(a),
                           o.third()->clone(a),
                           o.fourth()->clone(a)) { 
    }
};

template<class T1, class T2, class T3, class T4, class T5>
class compose_state5 : public compose_nstate_t {
public:
    typedef compose_state5<T1, T2, T3, T4, T5> this_state_t;

    compose_state5(mc_arena a, sm_t *sm)
        : compose_nstate_t(a, sm, new (a) T1(a, sm), new (a) T2(a, sm),
                                  new (a) T3(a, sm), new (a) T4(a, sm),
                                  new (a) T5(a, sm)) { }

    compose_state5(mc_arena a, sm_t *sm,
                   T1 *s1, T2 *s2, T3 *s3, T4 *s4, T5 *s5)
        : compose_nstate_t(a, sm, s1, s2, s3, s4, s5) { }

    T1* first() const { return static_cast<T1*>(get_nth_state_unsafe(0)); }
    T2* second() const { return static_cast<T2*>(get_nth_state_unsafe(1)); }
    T3* third() const { return static_cast<T3*>(get_nth_state_unsafe(2)); }
    T4* fourth() const { return static_cast<T4*>(get_nth_state_unsafe(3)); }
    T5* fifth() const { return static_cast<T5*>(get_nth_state_unsafe(4)); }

    state_t* clone(mc_arena a) const {
        state_t *rv = new (a) this_state_t(a, sm, *this);
        return rv;
    }

protected:
    // Don't use this constructor except for cloning.
    compose_state5(mc_arena a, sm_t *sm, const this_state_t &o)
        : compose_nstate_t(a, sm,
                           o.first()->clone(a),
                           o.second()->clone(a),
                           o.third()->clone(a),
                           o.fourth()->clone(a),
                           o.fifth()->clone(a)) {
    }
};

#endif // __COMPOSE_NSTATE_H__
