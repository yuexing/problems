/**
 * \file set-state.hpp
 *
 * State that's a set of objects.
 * 2 possible caching strategies: continue if we see a new object, or
 * continue if we fail to see an object that was always there.
 *
 * (c) 2012-2013 Coverity, Inc. All rights reserved worldwide.
 **/
#ifndef SET_STATE_HPP
#define SET_STATE_HPP

#include "store.hpp" // ShallowCopy

#include "libs/containers/set.hpp" // set
#include "libs/debug/output.hpp"   // GDOUT

template<typename T,
         typename Cmp,
         typename Copy = ShallowCopy<T>
    > struct set_state_t:
    public state_t {
    // Types
    typedef arena_set_t<T, Cmp> set_type;

    // Cache for set type
    struct cache_t: public ::cache_t {

        cache_t(arena_t &ar,
                bool continue_if_new_object,
                const char *object_desc):
            continue_if_new_object(continue_if_new_object),
            object_desc(object_desc),
            s(&ar) {
        }

        bool check_cache_hit
        (state_t *st,
         abstract_interp_t &cur_traversal,
         const cfg_edge_t *edge,
         bool widen,
         bool first_time)
        {
            set_state_t *state =
                static_cast<set_state_t *>(st);
            if(first_time) {
                // First time, just copy the state.
                foreach(i, state->s) {
                    s.insert(*i);
                }
                return false;
            }
            if(debug::caching) {
                StateWriter writer(cout);
                writer << "\nCache for " << object_desc << " ("
                       << state->sm->get_sm_name() << "):\n";
                write_as_text(writer);
                writer << '\n';
                state->write_as_text(writer);
            }
            bool rv = true;
            if(continue_if_new_object) {
                foreach(i, state->s) {
                    if(s.insert(*i).second) {
                        // New object
                        GDOUT(caching, *i
                              << " is not in cache, cache miss"
                        );
                        rv = false;
                    }
                }
            } else {
                for(LET(i, s.begin());
                    i != s.end(); /**/) {
                    if(!contains(state->s, *i)) {
                        GDOUT(caching, *i
                              << " is not in current state, cache miss"
                        );
                        s.erase(i++);
                        rv = false;
                    } else {
                        ++i;
                    }
                }
            }
            return rv;
        }
        void write_as_text(IndentWriter &out) const {
            out << object_desc << " in cache:\n";
            out.indent();
            foreach(i, s) {
                out << *i << '\n';
            }
            out.dedent();
        }

        // Indicates caching strategy
        bool const continue_if_new_object;
        // Describes the type of object in the set, for debugging.
        const char * const object_desc;
        set_type s;
    };

    // Functions
    set_state_t(arena_t *ar,
                sm_t *sm,
                bool continue_if_new_object,
                const char *object_desc):
        state_t(ar, sm),
        continue_if_new_object(continue_if_new_object),
        object_desc(object_desc),
        s(ar) {
    }

    override cache_t *create_empty_cache(arena_t *a) const
    {
        return new (*a) cache_t(*a, continue_if_new_object, object_desc);
    }

    void copyFrom(arena_t *ar, const set_state_t &other)
    {
        Copy cp;
        foreach(i, other.s) {
            s.insert(cp(*i, ar));
        }
    }

    override set_state_t *clone(arena_t *ar) const
    {
        set_state_t *rv = new(*ar) set_state_t(
            ar,
            sm,
            continue_if_new_object,
            object_desc);
        rv->copyFrom(ar, *this);
        return rv;
    }

    void write_as_text(StateWriter &out) const {
        SW_WRITE_BLOCK(out, stringb(object_desc << " in state:"));
        foreach(i, s) {
            out << "  " << *i << '\n';
        }
    }

    // Data

    // Indicates caching strategy
    bool const continue_if_new_object;
    // Describes the type of object in the set, for debugging.
    const char * const object_desc;
    // The actual set.
    set_type s;
};

#endif // SET_STATE_HPP
