// vstore.h
//
// vstore is a state that maps integers or pointers by value to other
// integers or pointers.
//
// Written By: Andy Chou
// Date: 2/19/2004
//
// (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
//

#ifndef __PSTORE_H__
#define __PSTORE_H__

#include "arena/arena-containers.hpp" // arena_map_t
#include "sm/sm.hpp"
#include "state.hpp"
#include "caching/empty-cache.hpp"
#include "text/stream-utils.hpp" // spaces

#include "text/sstream.hpp"
#include "libs/containers/map.hpp"

/**
 * class vstore_t<Key, Val>
 *
 * A generic store maps type Key to type Val.  Unlike store_t<>, it does
 * not assume that Key is "tree" -- but it also does not structurally
 * compare trees.  All comparisons (including of values) are done by
 * value (operator== and operator<), and all hashing done by casting
 * to size_t.
 *
 * Other template arguments:
 * A = allocator for pair of Key and Val for map
 *
 * A vstore doesn't have a cache (empty cache)
 *
 **/
//template<class Key, class Val,
template<class Key, class Val, class C = std::less<Key> >
class vstore_t : public state_t {
public:
    // STL-compatible types
    typedef Key key_type;
    //typedef Val store_data_type;
    typedef std::pair<const Key, Val> value_type;
    typedef size_t size_type;
    // ... missing many

    typedef vstore_t<Key, Val, C> this_store_t;
    typedef arena_map_t<Key, Val, C> store_map;
    typedef typename store_map::iterator iterator;
    typedef typename store_map::const_iterator const_iterator;



    vstore_t(mc_arena a, sm_t *sm) :
        state_t(a, sm),
        store(a)
    {
    }

    vstore_t(mc_arena a, sm_t *sm, const this_store_t &o) 
        : state_t(a, sm), store(o.store, a) { }

    cache_t *create_empty_cache(mc_arena a) const {
        return new (a) empty_cache_t;
    }

    void insert(const Key &key, const Val &val, bool replace = true) {
        if(replace) {
            store.erase(key);
        }
        store.insert(make_pair(key, val));
    }

    void clear() {
        store.clear();
    }

    void erase(const Key &key) {
        store.erase(key);
    }

    void erase(iterator i) {
        store.erase(i);
    }

    iterator find(const Key &key) {
        return store.find(key);
    }

    bool contains(const Key &key) {
        return ::contains(store, key);
    }

    const_iterator find(const Key &key) const {
        return store.find(key);
    }

    bool has(const Key &key, const Val& val) { 
        iterator p = find(this->t);
        return p != end() && p->second == val;
    }

    bool is_equal(const state_t& oth) const { 
        this_store_t& other = (this_store_t&) oth;
        return *store == *other.store;
    }

    state_t* clone(mc_arena a) const {
        return new (a) this_store_t(a, sm, *this);
    }

    iterator begin() { return store.begin(); }
    iterator end() { return store.end(); }

    const_iterator begin() const { return store.begin(); }
    const_iterator end() const { return store.end(); }

    size_type size() const { return store.size(); }

    Val& operator[](Key t) { 
        return store[t];
    }

    string value_at_id_string(size_t id) const {
        ostringstream return_ss;
        return_ss << (Val)id;
        return return_ss.str();
    }


    void write_as_text(StateWriter& out) const {
        out.openStateBlock(stringb("VSTORE: (" << sm->get_sm_name() << ")"));
        const_iterator p;
        for(p = begin(); p != end(); ++p) {
            out << p->first << " -> " << p->second << endl;
        }
        out.closeStateBlock("END VSTORE");
    }

private:
    // not assignable or copy-constructible for now
    vstore_t& operator=(const vstore_t &other);
    vstore_t(const vstore_t &other);

    store_map store;
};

#endif  // __VSTORE_H__
