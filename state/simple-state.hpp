// (c) 2004,2006-2007,2009-2012 Coverity, Inc. All rights reserved worldwide.


#ifndef __SIMPLE_STATE_H__
#define __SIMPLE_STATE_H__

#include "caching/eqcache.hpp"
#include "state.hpp"

template<class T>
class simple_state_t : public eq_state_t
{
public:
    simple_state_t( mc_arena a, sm_t *sm)
        : eq_state_t( a, sm ), value( T() ) {}
    simple_state_t( mc_arena a, sm_t *sm, T init_val )
        : eq_state_t( a, sm ), value( init_val ) {}

    simple_state_t( mc_arena a, sm_t *sm, const simple_state_t<T> &o )
        : eq_state_t( a, sm ), value( o.value ) {}

    state_t *clone( mc_arena a ) const {
        return new (a) simple_state_t<T>( a, sm, *this );
    }

    size_t compute_hash() const { return 0; }

    bool is_equal( const eq_state_t &other ) const {
        const simple_state_t &o = static_cast<const simple_state_t &>( other );
        return value == o.value;
    }

    void write_as_text(StateWriter &out) const {
        out << "Simple state: " << value << endl;
    }

    void set_value( T v ) { value = v; }
    T get_value() const { return value; }
    T &value_ref() { return value; }

private:
    T value;
};

#endif
