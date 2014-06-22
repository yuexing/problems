// subset_cache.h
//
// Interface to a subset cache, which is a simple cache useful for local
// analyses.
//


//
// 
//

#ifndef AST_SUBSET_CACHE_HPP
#define AST_SUBSET_CACHE_HPP

#include "containers/set.hpp"
#include "functional.hpp"  // less

#include "caching.hpp"
#include "ast/ast-compare.hpp" // astnode_compare_t
#include "ast/cc_print.hpp"
#include "generic-subset-caching.hpp"

typedef generic_subset_cache_t<const Expression *,
                               astnode_compare_t>
subset_cache_type;

/**
 * Hash of unique tree/value pairs for the subset cache.
 * The "size_t" pair uniquely identifies the value associated with the
 * tree. See analysis/sm/state/unique.hpp
 *
 * Using hash sets appears to be substantially slower
 **/
typedef subset_cache_type::subset_hash_t subset_hash_t;

typedef subset_cache_type::cache_pair_set_t
subset_cache_pair_set_t;

// Return the range of key/value pairs in "subset_cache_values_t" which have
// "expr" as key.
pair<
    subset_cache_pair_set_t::const_iterator,
    subset_cache_pair_set_t::const_iterator>
valuesWithExpr(const subset_cache_pair_set_t &cache, const Expression *expr);

// class subset_cache_t
//
// A cache where a hit is determined by a subset relationship.
//
class subset_cache_base_t : public cache_t
{
public:
    friend class generic_subset_cache_t<const Expression *,
                                        astnode_compare_t>;
    subset_cache_base_t
    (arena_t *ar,
     bool novalue_is_bottom,
     unsigned int eq_threshold,
     unsigned int seq_threshold):
        novalue_is_bottom(novalue_is_bottom),
        equality_caching_threshold(eq_threshold),
        slice_equality_caching_threshold(seq_threshold),
        subset_ht(ar),
        multi_ht(ar)
    { }

    // Check for a cache hit in the subset cache.
    bool check_cache_hit
    (state_t* state,
     abstract_interp_t &cur_traversal,
     const cfg_edge_t *edge,
     bool widen,
     bool first_time);

private:

    /**
     * If "true", the absence of a value is considered to be "bottom",
     * i.e. it generates a cache miss. This is the "subset"
     * behavior. Otherwise, it's considered "top", and generates a
     * cache hit ("superset" behavior"). In general, checkers should
     * set this to true and FPPs to false.
     **/
    bool const novalue_is_bottom;

    /**
     * If non-0, use "equality" caching until the cache reaches this
     * size, after which, use subset-caching.
     **/
    unsigned int const equality_caching_threshold;

    /**
     * If non-0 (and equality_caching_threshold is 0), use "equality" caching
     * on expressions specified by the SM until the equality cache reaches this
     * size, after which, use subset-caching.
     **/
    unsigned int const slice_equality_caching_threshold;

    /**
     * Set of seen tree/value pairs
     * With subset caching, represents the union of all the states
     * seen so far.
     **/
    subset_cache_type::cache_pair_set_t subset_ht;
    /**
     * Set of caches; each one represents a single state seen at the
     * cache point.
     **/
    subset_cache_type::multi_subset_hash_t multi_ht;
};

class subset_cache_t: public subset_cache_base_t {
public:
    subset_cache_t(arena_t *ar,
                   unsigned int eq_threshold = equality_caching_threshold_default,
                   unsigned int seq_threshold = slice_equality_caching_threshold_default):
        subset_cache_base_t(ar,
                            true,
                            eq_threshold, seq_threshold){}
};

class superset_cache_t: public subset_cache_base_t {
public:
    superset_cache_t(arena_t *ar,
                     unsigned int eq_threshold = equality_caching_threshold_default,
                    unsigned int seq_threshold = slice_equality_caching_threshold_default):
        subset_cache_base_t(ar,
                            false,
                            eq_threshold, seq_threshold){}
};

// This is the default value for the "equality caching limit". If this
// is not 0, a cache will use "equality caching" (instead of
// variable-indepedent a.k.a. "subset") until it reaches that many
// entries, after which it will collapse to "subset"
extern unsigned equality_caching_threshold_default;

// This is the default value for "selective equality caching", which is used
// when we're not using full-blown equality caching.
extern unsigned slice_equality_caching_threshold_default;

#endif // AST_SUBSET_CACHE_HPP
