// (c) 2007-2010,2012-2013 Coverity, Inc. All rights reserved worldwide.
/**
 * \file unique.hpp
 *
 * A file to make "values" (e.g. scalars, pointers) unique, allowing
 * shallow comparison and getting a unique number.
 *
 * This allows allowing only size_t as values in subset caching (so
 * subset_cache_t is not a template)
 *
 **/
#ifndef UNIQUE_STORE_VALUE_HPP
#define UNIQUE_STORE_VALUE_HPP

#include "containers/hash_map.hpp"
#include "containers/hash_set.hpp"
#include "containers/vector.hpp"
#include "functional.hpp"
#include "ast/ast-compare.hpp" // astnode_hash_t

#include "arena/arena.hpp"
#include "arena/arena-vector.hpp"
#include "exceptions/assert-lean.hpp"

/**
 * Clear all the instances of unique_t's
 * Only one instance of a given type is needed at a given time.
 * Free after analyzing a function.
 **/
void clear_unique_objs();

extern vector<void (*)(void)>
clear_unique_objs_instances_vec;

/**
 * Arena on which all those things will be allocated
 * (for convenient accounting; also includes a memory cap)
 **/
extern arena_t *get_unique_obj_arena();

template<typename X> struct unique_alloc_t {
    typedef arena_fn_alloc_t<X, get_unique_obj_arena> _;
};

/**
 * Use function-wide singleton instances.
 * Hash tables are big, and many different stores might want to use
 * the same kind
 * Also, in the case of killpaths filtering, those tables would be
 * leaked until the end of the function.
 * Killpaths filtering also means the function arena might change
 * within the analysis of a single function, so allocate all this on
 * a specific arena.
 *
 * clear_unique_objs() will clear all the allocated tables.
 **/
#define MAKE_GET_INSTANCE_NO_CTOR(x)                    \
    static x *get_instance() {                          \
        if(!the_instance) {                             \
            the_instance = new (get_unique_obj_arena())x;   \
            clear_unique_objs_instances_vec.            \
                push_back(clear_instance);              \
        }                                               \
        return the_instance;                            \
    }                                                   \
    protected:                                          \
    static x *the_instance;                             \
    static void clear_instance() {                      \
        the_instance = 0;                               \
    }                                                   \

#define MAKE_GET_INSTANCE(x)                            \
    MAKE_GET_INSTANCE_NO_CTOR(x)                        \
    x(){}                                               \

/**
 * Use this when there's no caching
 **/
template<typename T> struct invalid_unique_t {
    // These one are called by virtual functions (compute_hash, widen) and
    // therefore need to exist
    size_t get_unique_id(T t) {
        xfailure("Invalid cache");
        // Silence warning
        return 0;
    }
    T get_from_id(size_t id) {
        xfailure("Invalid cache");
        // Silence warning
        return T();
    }
    // Actually, I'd like the others to exist too so I can explicitly
    // instantiate some classes.
    static bool equals(T t1, T t2) {
        xfailure("Don't call");
    }
    MAKE_GET_INSTANCE(invalid_unique_t);
};

template<typename T> invalid_unique_t<T>* invalid_unique_t<T>::the_instance;

/**
 * Return a unique instance by storing the value in a set and
 * returning the value stored if already present. Uses the given
 * template parameters for a hash set.
 *
 * Returns a unique ID by casting (what is assumed to be a pointer) to
 * size_t. Reverses that by casting back to the pointer.
 *
 * The pointers must have at least fn arena lifetime.
 * If not, use hash_and_count_unique_t
 **/
template<typename T, typename H = hash<T>, typename Eq=equal_to<T> >
struct hash_and_cast_unique_t {
    hash_set<T, H, Eq, typename unique_alloc_t<T>::_ > cache;
    size_t get_unique_id(T t) {
        return (size_t)*cache.insert(t).first;
    }
    T get_from_id(size_t s) {
        return (T)s;
    }
    // Also useful to do this without a lookup
    static bool equals(T t1, T t2) {
        return Eq()(t1, t2);
    }
    MAKE_GET_INSTANCE(hash_and_cast_unique_t);
};

template<typename T, typename H, typename Eq>
hash_and_cast_unique_t<T, H, Eq>* hash_and_cast_unique_t<T, H, Eq>::the_instance;

typedef hash_and_cast_unique_t<const ASTNode *, astnode_hash_t, astnode_eq_t>
ast_unique_t;
typedef hash_and_cast_unique_t<const Expression *, astnode_hash_t, astnode_eq_t>
expr_unique_t;
typedef hash_and_cast_unique_t<const char *, hash_cstr, eqstr> string_unique_t;
/**
 * Return a unique instance by assuming it is already unique, and can
 * be cast to size_t. Use e.g. for unique pointers or enums.
 **/
template<typename T>
struct cast_unique_t {
    size_t get_unique_id(T t) {
        return (size_t)t;
    }
    T get_from_id(size_t s) {
        return (T)s;
    }
    // Also useful to do this without a lookup
    static bool equals(T t1, T t2) {
        return t1 == t2;
    }
    MAKE_GET_INSTANCE(cast_unique_t);
};

template<typename T>
cast_unique_t<T>* cast_unique_t<T>::the_instance;

typedef cast_unique_t<int> int_unique_t;

/**
 * Doesn't actually make unique, considers all elements to be equal.
 * Use it if the value is not supposed to matter for caching.
 **/
template<typename T>
struct const_unique_t {
    size_t get_unique_id(T t) {
        return (size_t)0;
    }
    T get_from_id(size_t s) {
        return T();
    }
    // Also useful to do this without a lookup
    static bool equals(T t1, T t2) {
        return true;
    }
    MAKE_GET_INSTANCE(const_unique_t);
};

template<typename T>
const_unique_t<T>* const_unique_t<T>::the_instance;

/**
 * Return a unique instance by storing the value in a map and
 * returning the value stored (as key) if already present. Uses the given
 * template parameters for a hash map.
 *
 * Returns a unique ID by generating it, reverses by storing the
 * reverse mapping (in a vector).
 *
 * Copy is a functor that takes the unique obj arena (as an arena_t
 * *) and a const T & and returns a T with data allocated on that arena (or
 * longer lifetime, e.g. fnscope). Same as the Copy argument of
 * primitive_syn_store_t.
 **/
template<typename T,
         typename Copy,
         typename H, typename Eq>
struct hash_and_count_unique_t {
    hash_map<T, size_t, H, Eq,
             typename unique_alloc_t<pair<T const, size_t> >::_ > cache;
    // Store only pointers in the reverse mapping, ownership is in cache.
    arena_vector_t<const T *> rev;
    const pair<T const, size_t> &get_unique_pair(const T &t) {
        LET(it, cache.find(t));
        if(it != cache.end()) {
            return *it;
        }
        T cl = Copy()(t, get_unique_obj_arena());
        const pair<T const, size_t> &rv =
            *cache.insert(make_pair(cl, cache.size())).first;
        // Added
        rev.push_back(&rv.first);
        return rv;
    }
    size_t get_unique_id(const T &t) {
        return get_unique_pair(t).second;
    }
    const T &get_from_id(size_t s) {
        return *rev[s];
    }
    // Also useful to do this without a lookup
    static bool equals(const T &t1, const T &t2) {
        return Eq()(t1, t2);
    }
    MAKE_GET_INSTANCE_NO_CTOR(hash_and_count_unique_t);
protected:
    hash_and_count_unique_t():
        rev(get_unique_obj_arena()) {
    }
};

template<typename T, typename Copy, typename H, typename Eq>
hash_and_count_unique_t<T, Copy, H, Eq>* hash_and_count_unique_t<T, Copy, H, Eq>::the_instance;

#endif
