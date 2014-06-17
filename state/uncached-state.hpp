// (c) 2013 Coverity, Inc. All rights reserved worldwide.

#ifndef __EMPTY_STATE_H__
#define __EMPTY_STATE_H__

#include "caching/empty-cache.hpp"
#include "state.hpp"

// This is a simple templatized state that is not cached.
template<class T>
class uncached_state_t : public state_t
{
public:
    uncached_state_t( mc_arena a, sm_t *sm)
        : state_t( a, sm ), value( T() ) {}
    uncached_state_t( mc_arena a, sm_t *sm, T init_val )
        : state_t( a, sm ), value( init_val ) {}

    uncached_state_t( mc_arena a, sm_t *sm, const uncached_state_t<T> &o )
        : state_t( a, sm ), value( o.value ) {}

    override cache_t *create_empty_cache( mc_arena a ) const {
        return new (a) empty_cache_t(a);
    }

    override state_t *clone( mc_arena a ) const {
        return new (a) uncached_state_t<T>( a, sm, *this );
    }

    override void write_as_text(StateWriter &out) const {
        out << "Uncached state: " << value << endl;
    }

    void set_value( T v ) { value = v; }
    T get_value() const { return value; }
    T &value_ref() { return value; }

private:
    T value;
};

#endif
