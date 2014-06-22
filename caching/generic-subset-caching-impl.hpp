/**
 * generic-subset-caching.hpp
 *
 * Implementation of "subset" caching.
 *
 * "Subset" caching works this way:
 * A state can be converted into a mapping from some keys to some
 * values.
 * The cache rembers, for each key, all the values that have been seen
 * associated with it.
 * A "cache hit" happens if, for each key, the corresponding value has
 * already been seen associated with it.
 *
 * The "novalue_is_bottom" flag allows deciding whether the presence
 * of a value if considered to encompass all values ("top", typical FPP
 * behavior) or is considered to represent no value at all ("bottom",
 * typical checker behavior).
 *
 * The values are always normalized to integers; in this file the keys
 * are a template parameter.
 *
 * Written By: Seth Hallem
 * Date: May 23, 2003
 *
 * 
 **/

#include "sm/sm.hpp"
#include "sm/state/state.hpp"

#include "libs/debug/output.hpp" // GDOUT
#include "text/printed-counter.hpp"
#include "time/activity-path-timer.hpp"
#include "exceptions/assert.hpp" // ostr_assert

#include "generic-subset-caching.hpp"

#include "ast/ast-print-ops.hpp" // For debugging, when key_type == const
                            // ASTNode *

template<typename key_type,
         typename key_compare>
bool
generic_subset_cache_t<key_type, key_compare>
::check_insert_set
(const subset_hash_t &state,
 multi_subset_hash_t &eq_cache,
 arena_t *cache_arena)
{
    DEF_ACTIVITY("Subset caching: eq caching");

    LET(i, eq_cache.find(const_cast<subset_hash_t *>(&state)));
    if(i == eq_cache.end()) {
        // Not already there; add.
        // First clone onto a longer-lasting arena.
        eq_cache.insert(
            subset_hash_t::create(
                state,
                cache_arena
            )
        );
        // Cache miss.
        return false;
    }
    // Cache hit.
    return true;
}

// generic_check_cache_hit
//
// Check for a hit in the subset cache.  Do so by converting the state
// machine state into a set of 'size_t's, then determining if that set of
// 'size_t's is a subset of the 'size_t's already in the cache.
//
template<typename key_type,
         typename key_compare>
bool
generic_subset_cache_t<key_type, key_compare>
::check_cache_hit
(const subset_hash_t &current_state,
 unsigned int equality_caching_threshold,
 cache_pair_set_t &current_cache,
 multi_subset_hash_t &current_eq_cache,
 bool first_time,
 bool novalue_is_bottom,
 arena_t *cache_arena)
{
    if (current_eq_cache.size() < equality_caching_threshold) {
        DEF_ACTIVITY("Subset caching: eq caching");

        if(!check_insert_set(
               current_state,
               current_eq_cache,
               cache_arena)) {
            if(current_eq_cache.size() == equality_caching_threshold) {
                // Reached limit.
                // Turn into a subset cache by collapsing all the sets
                // into one.
                foreach(i, current_eq_cache) {
                    current_cache.insert((*i)->begin(), (*i)->end());
                }
            }
            return false;
        } else {
            return true;
        }
    }
    bool rv = true;

    static printed_counter total_tuples("Total cache tuples");
    if(first_time) {
        // First time, fill the state
        // Otherwise we'd always hit if no value means "top" (since at
        // first we don't have a value for anything, this would mean
        // "top" for all keys)
        foreach(i, current_state) {
            current_cache.insert(*i);
            ++total_tuples;
        }
        return false;
    }
    if(novalue_is_bottom) {
        DEF_FREQUENT_ACTIVITY("Subset caching: adding to cache");
        foreach(i, current_state) {
            ++total_tuples;
            if(current_cache.insert(*i).second)
                rv = false;
        }
    } else {
        // No value means top. This is more complicated, since absence
        // is meaningful.
        DEF_FREQUENT_ACTIVITY("Superset caching");
        LET(i1, current_state.begin());
        LET(i2, current_cache.begin());
        
        while(i2 != current_cache.end()) {
            int cmp;
            if(i1 == current_state.end() ||
               ((cmp = key_compare()(i2->first, i1->first)) < 0)) {
                // cache's key is not in current state
                // => cache miss (state is more general, keep it)
                // Also, remove key, to indicate we've seen "top"
                GDOUT(caching, "Key " << i2->first
                      << " is missing, cache miss");

                // This is the first (and only) time we'll see this
                // i2->first, so no need to look for a previous value
                // with the same "first"
                // So i2 == h2.lower_bound(make_pair(i2->first, (size_t)0))
                
                // Find first value whose "first" is after
                // i2->first
                LET(new_i2, current_cache.upper_bound(make_pair(i2->first, (size_t)-1)));
                // Remove all values with same "first"
                current_cache.erase(i2, new_i2);
                i2 = new_i2;
                rv = false;
            } else if(cmp > 0) {
                // current state's key is not in cache
                // Means that value is already covered by the implicit
                // "top" for that key
                GDOUT(caching, "Missing key " << i1->first);
                ++i1;
            } else {
                GDOUT(caching, "Key " << i1->first << " present in both");
                if(current_cache.insert(*i1).second) {
                    GDOUT(caching, "Added value, cache miss");
                    // Cache miss if we inserted a value
                    rv = false;
                }
                // Skip all values with current key, otherwise we'll
                // hit the case above
                i2 = current_cache.upper_bound(make_pair(i1->first, (size_t)-1));
                ++i1;
            }
        }
    }

    return rv;
}

template <typename key_type, typename key_compare>
struct subset_hash_key_lt_t {
    bool operator()(const key_type &a, const key_type &b) const {
        return key_compare()(a, b) < 0;
    }
};

template<typename key_type,
         typename key_compare>
bool
generic_subset_cache_t<key_type, key_compare>
::check_slice(const subset_hash_t &state,
              unsigned slice_threshold,
              multi_subset_hash_t &eq_cache,
              const VectorBase<key_type> *slice,
              arena_t *cache_arena)
{
    if (slice && eq_cache.size() < slice_threshold) {
        stack_allocated_arena_t ar("check_slice");
        subset_hash_t ht(&ar);
        set<key_type, subset_hash_key_lt_t<key_type, key_compare> > s(slice->begin(), slice->end());
        foreach (it, state) {
            if (contains(s,it->first) ) {
                ht.push_back(*it);
            }
        }

        return check_insert_set(ht, eq_cache, cache_arena);
    }
    return true;
}

template<typename key_type,
         typename key_compare>
template<typename state_type,
         typename cache_type>
bool
generic_subset_cache_t<key_type, key_compare>
::check_cache_hit
(state_t *sm_st,
 cache_type *cache,
 const VectorBase<key_type> *slice,
 bool widen,
 bool first_time,
 bool novalue_is_bottom,
 arena_t *cache_arena)

{
    state_type *state = dynamic_cast<state_type*>(sm_st);
    ostr_assert(state, "state for SM " << sm_st->sm->get_sm_name()
                << " used with subset cache but not subset state");
    bool rv;
    bool use_eq_cache =
        (cache->multi_ht.size() < cache->equality_caching_threshold) ||
        (slice && cache->multi_ht.size() < cache->slice_equality_caching_threshold);

    sm_t *sm = sm_st->sm;
    // Add a sub-timer with constant name for later aggregation
    DEF_FREQUENT_ACTIVITY("Caching");
    stack_allocated_arena_t ar("subset cache");

    subset_cache_pair_set_t &current_cache = cache->subset_ht;
    
    if(debug::caching) {
        StateWriter writer(cout);
        writer << endl << "Cache for: (" <<
            sm->get_sm_name() << ")" << endl;
        if(use_eq_cache) {
            writer << "Eq cache: caches =\n";
            foreach(i, cache->multi_ht) {
                writer << "-- cache --\n";
                writer.indent();
                state->print_cache(writer, **i);
                writer << '\n';
                writer.dedent();
            }
            writer << "-- end eq caches --\n";
        }
        if (!use_eq_cache || cache->equality_caching_threshold==0) {
            state->print_cache(writer, current_cache);
            writer << endl;
        }
        state->write_as_text(writer);
    }

    // This is subtle: we always widen.  Why?  It's actually faster
    // this way because of the interaction with compose_ncache.  This
    // allows compose_ncache to only do 1 pass over all of the
    // subcaches, instead of waiting until a cache miss, then
    // re-iterating over each subcache again to say "some other cache
    // missed, so you must now widen".
    // Also, widen *before* checking for cache so that the widening may
    // help hit (DR 5300)
    if(widen && state->widens()) {
        DEF_FREQUENT_ACTIVITY("Widening");
        if(use_eq_cache) {
            foreach(i, cache->multi_ht) {
                state->widen(**i);
            }
        }
        if (!use_eq_cache || cache->equality_caching_threshold==0) {
            state->widen(current_cache);
        }
    }

    // 1. Convert State to a get a mapping to size_t

    subset_hash_t current_state(&ar);
    {
        START_STOP_ACTIVITY(sm->hash_timer());
        // Add a sub-timer with constant name for later aggregation
        DEF_FREQUENT_ACTIVITY("Hashing");
        state->compute_hash(current_state);
    }

    // 2. Run the subset caching algorithm
    rv = check_cache_hit
        (current_state,
         cache->equality_caching_threshold,
         current_cache,
         cache->multi_ht,
         first_time,
         novalue_is_bottom,
         cache_arena
         );
    
    // 2. Run selective equality caching if we're not already running full-on
    // equality caching
    if (cache->equality_caching_threshold == 0) {
        // Populate this cache even if the previous check resulted in a miss.
        bool tmp = check_slice(current_state,
                               cache->slice_equality_caching_threshold,
                               cache->multi_ht,
                               slice,
                               cache_arena);
        rv = rv && tmp;
    }

    if (debug::caching) {
        cout << "result: " << (rv ? "HIT" : "MISS") << endl;
    }
    return rv;
}
