// 
#include "analysis/caching/generic-subset-caching-impl.hpp"
#include "analysis/traversal/abstract-interp.hpp"

struct int_compare_t {
    int operator()(int i1, int i2) const {
        if(i1 < i2)
            return -1;
        if(i1 > i2)
            return 1;
        return 0;
    }
};

typedef generic_subset_cache_t<int, int_compare_t > caching;
typedef caching::subset_hash_t state_type;
typedef caching::cache_pair_set_t cache_type;
typedef pair<int, size_t> p;

void caching_unit_tests() {
    SAVE_RESTORE(debug::caching, true);
    stack_allocated_arena_t ar("caching test");

    cache_type::allocator_type alloc(&ar);

    caching::multi_subset_hash_t eqcache(&ar);

    // "caching" data structures are allocated on the current abstract
    // interp
    abstract_interp_t interp;
    
    SAVE_RESTORE(subset_cache_arena, &ar);

    state_type state(&ar);
    cache_type cache(&ar);

    // This value is used in an upper_bound call, make sure that works
    size_t const max_size_t = (size_t)-1;

    state.push_back(p(1, 0));
    state.push_back(p(2, 0));
    state.push_back(p(3, 0));

    cout << "Initializing cache" << endl;

    caching::check_cache_hit(state,
                             0,
                             cache,
                             eqcache,
                             /*first*/true,
                             false,
                             &ar);

    cond_assert(cache.size() == 3);
    cond_assert(contains(cache, p(1,0)));
    cond_assert(contains(cache, p(2,0)));
    cond_assert(contains(cache, p(3,0)));

    state.clear();
    state.push_back(p(2, max_size_t));

    cout << "First state, new value for 2, cache miss" << endl;

    bool rv;
    rv = caching::check_cache_hit(state,
                             0,
                             cache,
                             eqcache,
                             /*first*/false,
                             false,
                             &ar);
    cond_assert(!rv);

    cond_assert(cache.size() == 2);
    cond_assert(contains(cache, p(2,0)));
    cond_assert(contains(cache, p(2,max_size_t)));

    state.clear();
    state.push_back(p(1, 2));

    cout << "Second state, no value for 2, cache miss" << endl;

    rv = caching::check_cache_hit(state,
                                  0,
                                  cache,
                                  eqcache,
                                  /*first*/false,
                                  false,
                                  &ar);

    cond_assert(!rv);
    cond_assert(cache.empty());

    state.clear();

    state.push_back(p(0, 0));
    state.push_back(p(1, 0));
    state.push_back(p(2, max_size_t));

    cout << "Reinitializing cache" << endl;

    caching::check_cache_hit(state,
                             0,
                             cache,
                             eqcache,
                             /*first*/true,
                             false,
                             &ar);

    state.clear();
    state.push_back(p(2, max_size_t));

    cout << "Third state, no value for 0 and 1, cache miss" << endl;

    rv = caching::check_cache_hit(state,
                             0,
                             cache,
                             eqcache,
                             /*first*/false,
                             false,
                             &ar);
    cond_assert(!rv);

    cond_assert(cache.size() == 1);
    cond_assert(contains(cache, p(2, max_size_t)));

}
