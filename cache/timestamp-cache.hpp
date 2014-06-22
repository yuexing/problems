/**
 * \file timestamp-cache.hpp
 *
 * A timestamp-based cache:
 *
 * The idea is to speed-up frequent lookups by using an in-memory map
 *
 * The algorithm is the following:
 * The cache keeps a global "timestamp", which is incremented on each
 * lookup. Also, each mapping has an associated timestamp, which
 * corresponds to last access:
 * - If a lookup succeeds, the timestamp is updated to the current
 * If a lookup fails, the value is fetched using the slower query
 * (often a DB lookup), and inserted, along with the current
 * timestamp. If this causes the map to get to a specified
 * threshold, then the oldest-accessed 1/2 mappings are discarded. The
 * amortized cost of this operation is the cost of a single erase +
 * log (max size)
 *
 * Note that when a lookup is done, a mapping must exist. If mappings
 * are optional, this needs to be reflected in the data type.
 *
 **/
// Given a heap with a potentially invalid top element, turn it back
// into a heap by potentially moving that element around.
// Equivalent to pop_heap + push_heap
template<typename It, typename Comp>
void replace_heap_top
(It b, It e, Comp lt) {
    size_t const size = e - b;
    size_t cur_idx = 0;
    size_t child = cur_idx * 2 + 1;

    while(child < size) {
        // Rebalance the heap, where "b[cur_idx]" may have been
        // changed.

        bool c1_smaller = lt(b[child], b[cur_idx]);
        // If child 2 is invalid, consider it smaller than current
        // value, in which case we won't try to mess with it.
        bool c2_smaller = (child + 1 >= size) ||
            lt(b[child + 1], b[cur_idx]);
        size_t child_to_replace;
        if(c1_smaller == c2_smaller) {
            if(c1_smaller) {
                // parent bigger than both children; it's a heap.
                return;
            } else {
                // Parent smaller than both children.
                // Swap "cur" and bigger child.
                // Put bigger child in "child_to_replace"
                if(lt(b[child], b[child + 1])) {
                    child_to_replace = child + 1;
                } else {
                    child_to_replace = child;
                }
            }
        } else {
            // Parent smaller than only 1 child; find which one and
            // swap with it, it will automatically be bigger than the
            // other child.
            if(c2_smaller) {
                // Means child 1 is bigger than current. Swap with it.
                child_to_replace = child;
            } else {
                child_to_replace = child + 1;
            }
        }
        std::swap(b[child_to_replace], b[cur_idx]);
        cur_idx = child_to_replace;
        child = cur_idx * 2 + 1;
    }
}

OPEN_NAMESPACE(timestamp_cache_details);

template<typename Data> struct entry_t {
    typedef Data mapped_type;
    Data data;
    size_t timestamp;
};

CLOSE_NAMESPACE(timestamp_cache_details);

// A cache with a custom type as the underyling map; with a
// requirement that the map's mapped_type is a
// timestamp_cache_details::entry_t
template<typename Map>
struct generic_timestamp_cache_t:
    public cache_base_t {

    typedef Map cache_map_t;

    typedef typename Map::mapped_type entry_t;

    typedef typename Map::key_type key_type;
    typedef typename entry_t::mapped_type mapped_type;
    typedef pair<const key_type, entry_t> value_type;
    
    
    typedef typename cache_map_t::iterator cache_iterator;
    struct compare_cache_iterator_t {
        bool operator()(const cache_iterator &i1,
                        const cache_iterator &i2) const {
            // We want higher number at the top of the pqueue.
            return i1->second.timestamp < i2->second.timestamp;
        }
    };
    generic_timestamp_cache_t
    (const char *name,
     size_t max_size):
        cache_base_t(name, max_size),
        m_timestamp(0),
        m_to_delete() {}

    // Return a modifiable value.
    // A possible use is for a cache that may or may not allow
    // "negative" results. If a negative result is returned from
    // cache, it can be replaced with a new "positive" value.
    // Also allows seeding the cache: with "call_inner_get_data"
    // false, this will create a mapping, but not actually call
    // inner_get_data. If "hit" is not NULL, it will indicate whether
    // a value was already in cache.
    mapped_type &get_data_ref
    (const key_type &key,
     bool call_inner_get_data,
     bool *hit) {
        pair<cache_iterator, bool> p
            = m_cache.insert(make_pair(key, entry_t()));
        entry_t &entry = p.first->second;
        entry.timestamp = m_timestamp++;
        if(p.second) {
            if(call_inner_get_data) {
                try {
                    inner_get_data(key, entry.data);
                } catch(...) {
                    // If "inner_get_data" throws, then "entry.data"
                    // is probably invalid, and should not be kept.
                    m_cache.erase(p.first);
                    throw;
                }
                // If we don't call inner_get_data, assume we're
                // seeding the cache; don't consider this a "miss".
                ++m_miss_count;
            }
            clear_old_entries();
            if(hit)
                *hit = false;
        } else {
            ++m_hit_count;
            if(hit)
                *hit = true;
        }
        return entry.data;
    }

    mapped_type &get_data_ref
    (const key_type &key) {
        return this->get_data_ref
            (key,
             /*call_inner_get_data*/true,
             /*hit*/NULL);
    }
    
    const mapped_type &get_data(const key_type &key) {
        return get_data_ref(key);
    }
    
    // Current size of the cache
    size_t size() const {
        return m_cache.size();
    }

    void resize(size_t new_max_size) {
        m_max_size = new_max_size;
        clear_old_entries();
    }
    void clear() {
      foreach(i, m_cache) {
          about_to_remove(i->first, i->second.data);
      }
      m_cache.clear();
    }
    
    size_t max_size() const {
        return m_max_size;
    }
    size_t timestamp() const {
        return m_timestamp;
    }

    size_t memory_size() const {
        size_t rv = 0;
        rv += map_memory(m_cache);
        rv += vector_memory(m_to_delete);
        if(value_type_has_memory()) {
            foreach(i, m_cache) {
                rv += value_type_memory_size(*i);
            }
        }
        return rv;
    }

    // Indicates whether key or data uses memory.
    virtual bool value_type_has_memory() const {
        return false;
    }
    // If value_type_has_memory() is true, indicates how much memory
    // the given value takes.
    virtual size_t value_type_memory_size
    (const value_type &val) const {
        return 0;
    }

    // Indicates whether it's allowed to remove the data from the
    // cache.
    // For a cache of ref-counted pointers, this may check whether the
    // data is unique()
    virtual bool allow_removal(const mapped_type &data) const {
        return true;
    }

    /**
     * Called just before a cached element is about to be evicted.  Allows
     * subtypes to override in case the data held by the element needs to be
     * flushed (e.g., to disk or DB)
     */
    virtual void about_to_remove(const key_type &key,
                                 mapped_type &data) const {}

    // Return the backing in-memory map (read-only)
    const cache_map_t &cache_map() const {
        return m_cache;
    }

    virtual ~generic_timestamp_cache_t(){}
protected:

    // The function uses need to override.
    // Performs the slow lookup that needs to be cached.
    virtual void inner_get_data
    (const key_type &key, mapped_type &data) = 0;

    // Remove entries that are too old, if the cache has become too
    // big.
    void clear_old_entries() {
        if(m_cache.size() < m_max_size) {
            return;
        }
        // In the case of "resize", cache.size() may not be exactly
        // equal to m_max_size.
        size_t elements_to_delete =
            m_cache.size() - (m_max_size / 2);
        
        // Find the botton max_size/2 elements.
        // To do that, we use a heap.
        // We add to the heap either if the heap is small
        // enough, or if the timestamp is older than the
        // most recent timestamp in the heap (which, due to
        // the heap structure, will end up at the front of the
        // vector)
        m_to_delete.reserve(elements_to_delete);
        foreach(i, m_cache) {
            if(!allow_removal(i->second.data)) {
                continue;
            }
            if(m_to_delete.size() < elements_to_delete) {
                // Skip the last added entry
                // It can only otherwise potentially be deleted if we
                // refuse to delete over 1/2 the entries.
                if(i->second.timestamp == m_timestamp - 1) {
                    continue;
                }
                // Simply add to the queue
                m_to_delete.push_back(i);
                std::push_heap
                    (m_to_delete.begin(),
                     m_to_delete.end(),
                     compare_cache_iterator_t());
            } else if(i->second.timestamp <
                      m_to_delete.front()->second.timestamp) {
                // Replace the top element with the new element:
                // remove top element
                m_to_delete.front() = i;
                replace_heap_top
                    (m_to_delete.begin(),
                     m_to_delete.end(),
                     compare_cache_iterator_t());
            }
        }
        foreach(i, m_to_delete) {
            about_to_remove((*i)->first, (*i)->second.data);
            m_cache.erase(*i);
        }
        m_to_delete.clear();
    }
    
    // Data.
    cache_map_t m_cache;
    // Current timestamp
    size_t m_timestamp;

    // Heap used to delete the oldest elements.
    // We don't use priority_queue because we want to access the
    // elements; also we use "replace_heap_top"
    // It's a member so we don't have to keep allocating it.
    vector<cache_iterator> m_to_delete;
};

// Timestamp cache, using an ordered map
template<typename Key,
         typename Data,
         typename Comp = std::less<Key>,
         typename Alloc = std::allocator<pair<const Key, Data> > >
struct timestamp_cache_t:
    public generic_timestamp_cache_t
<map<Key,
     timestamp_cache_details::entry_t<Data>,
     Comp,
     typename Alloc::template rebind
     <pair<const Key,
           timestamp_cache_details::entry_t<Data> > >::other> >
{
    timestamp_cache_t
    (const char *name,
     size_t size):
        generic_timestamp_cache_t
        <map
         <Key,
          timestamp_cache_details::entry_t<Data>,
          Comp,
          typename Alloc::template rebind
          <pair<const Key,
                timestamp_cache_details::entry_t<Data> > >::other> >
        (name, size) {
    }
};

template<typename Key,
         typename Data,
         typename Hash = hash<Key>,
         typename Eq   = std::equal_to<Key>,
         typename Alloc = std::allocator<pair<const Key, Data> > >
struct timestamp_hash_cache_t:
    public generic_timestamp_cache_t
<hash_map<Key,
          timestamp_cache_details::entry_t<Data>,
          Hash,
          Eq,
     typename Alloc::template rebind
     <pair<const Key,
           timestamp_cache_details::entry_t<Data> > >::other> >
{
    timestamp_hash_cache_t
    (const char *name,
     size_t size):
        generic_timestamp_cache_t
        <hash_map
         <Key,
          timestamp_cache_details::entry_t<Data>,
          Hash,
          Eq,
          typename Alloc::template rebind
          <pair<const Key,
                timestamp_cache_details::entry_t<Data> > >::other> >
        (name, size) {
    }
};

// Unit tests.
void timestamp_cache_unit_tests();

#endif // TIMESTAM_CACHE_HPP
