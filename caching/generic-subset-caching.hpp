// 
/**
 * \file generic-subset-caching.hpp
 *
 * Typedefs for subset caching
 *
 **/
#ifndef GENERIC_SUBSET_CACHING_HPP
#define GENERIC_SUBSET_CACHING_HPP

#include "arena/arena.hpp"
#include "arena/arena-vector.hpp"
#include "arena/arena-containers.hpp" // arena_set_t

#include "containers/pair.hpp"
#include "containers/set.hpp"
#include "libs/functional.hpp" // less

extern unsigned int equality_caching_threshold_default;
extern unsigned int slice_equality_caching_threshold_default;

template<typename key_type,
         typename key_compare>
struct generic_subset_cache_t {
    struct subset_hash_entry_lt_t {
        bool operator()(const pair<key_type, size_t> &p1,
                        const pair<key_type, size_t> &p2) const {
            if(int i = key_compare()(p1.first, p2.first))
                return i < 0;
            return p1.second < p2.second;
        }
    };

    /**
     * Hash of unique key_type/value pairs for the subset cache.
     * The "size_t" pair uniquely identifies the value associated with the
     * key_type. See analysis/sm/state/unique.hpp
     *
     * Using hash sets appears to be substantially slower.
     * Also hash sets are not suitable for no value as TOP
     * DCDC: Also, hash sets will cause nondeterminism issues if
     *   widen(widen(a, b), c) != widen(widen(a, c), b)
     * CHG: Changed to sorted vector for performance reasons.
     **/
    typedef arena_vector_t<pair<key_type, size_t> > subset_hash_t;

    typedef arena_set_t<
        pair<key_type, size_t>,
        subset_hash_entry_lt_t> cache_pair_set_t;

    struct subset_hash_lt_t {
        bool operator()(const subset_hash_t *h1,
                        const subset_hash_t *h2) const {
            return std::lexicographical_compare(h1->begin(), h1->end(),
                                                h2->begin(), h2->end(),
                                                subset_hash_entry_lt_t());
        }
    };
    typedef arena_set_t<subset_hash_t *,
                subset_hash_lt_t> multi_subset_hash_t;

    /**
     * Only does the subset caching part itself
     **/
    static bool check_cache_hit
    (const subset_hash_t &state,
     unsigned int equality_caching_threshold,
     cache_pair_set_t &cache,
     multi_subset_hash_t &eq_cache,
     bool first_time,
     bool novalue_is_bottom,
     arena_t *cache_arena);

    static bool check_slice(const subset_hash_t &state,
                            unsigned threshold,
                            multi_subset_hash_t &eq_cache,
                            const VectorBase<key_type> */*nullable*/slice,
                            arena_t *cache_arena);

    // Check whether "state" is already in "eq_cache", if so, return
    // "true", else add it (after cloning onto the cache arena) and
    // return "false".
    static bool check_insert_set(
        const subset_hash_t &state,
        multi_subset_hash_t &eq_cache,
        arena_t *cache_arena);

    /**
     * Does caching + widening
     **/
    template <typename state_type,
              typename cache_type>
    static bool check_cache_hit
    (state_t  *state,
     cache_type *cache,
     const VectorBase<key_type> */*nullable*/slice,
     bool widen,
     bool first_time,
     bool novalue_is_bottom,
     arena_t *cache_arena
     );
};

extern mc_arena subset_cache_arena;

#endif
