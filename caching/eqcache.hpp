// eqcache.h
//
// Equality cache.  This cache checks for membership of a state data
// structure in the cache using the equality comparator for that data
// structure.  The hash function is just a hash on the set of 'size_t's
// returned by the state's compute_hash function.  Unlike the subset
// cache, though, those values do not need to uniquely identify a
// particular state structure.  Only the combination of hash function and
// equality comparator needs to be truly unique.
//


//
// 
//

#ifndef __EQ_CACHE_H__
#define __EQ_CACHE_H__

// libs
#include "arena/arena.hpp"
#include "arena/arena-containers.hpp" // arena_hash_set_t

// analysis

#include "caching.hpp"

// FORWARD DECLARATIONS
class eq_state_t;

class eq_cache_hash_t
{
public:
    size_t operator()(const state_t* sm_st) const;
};

class eq_cache_equality_t
{
public:
    bool operator()(const state_t* sm_st1,
                    const state_t* sm_st2) const;
};

class eq_cache_t : public cache_t
{
public:
    eq_cache_t(arena_t *ar);

    // Check if 'sm_st' is already in the cache.
    bool check_cache_hit(state_t* state,
                         abstract_interp_t &cur_traversal,
                         const cfg_edge_t *edge,
                         bool widen,
                         bool first_time);

private:

    // The type/declaration of the cache data structure.
    typedef arena_hash_set_t<const eq_state_t*, eq_cache_hash_t, eq_cache_equality_t> eq_hash_t;
    eq_hash_t ht;
};

#endif // ifndef __EQ_CACHE_H__
