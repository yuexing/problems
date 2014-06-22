/**
 * \file cache-base.hpp
 *
 * A base class for a cache. Not a template, so allows uniform
 * handling of caches.
 *
 **/

// A base class with data that's common to all cache types, and that's
// useful for uniform handling of caches
class cache_base_t {
public:

    cache_base_t(const char *name,
                 size_t max_size);

    virtual ~cache_base_t();
    
    // Name of the cache, for printing stats
    string m_name;

    // Maximum size of the cache.
    // If it reaches that size, the oldest 1/2 mappings are removed.
    size_t m_max_size;

    // Number of times we hit the cache
    unsigned long m_hit_count;
    // Number of times we missed
    unsigned long m_miss_count;

    // A vector containing all the constructed caches
    // NULL if no caches have been constructed.
    static vector<cache_base_t *> *all_caches;

    // Obtain the maximum size of the cache
    size_t size() const {
        return m_max_size;
    }
    // Set the maximum size of the cache
    virtual void resize(size_t new_size) = 0;

    // Proportion of hits vs calls. 0 if no calls.
    double hit_rate() const;
    // Proportion of misses vs calls. 0 if no calls.
    double miss_rate() const;

    // Number of times the cache was called
    size_t call_count() const {
        return m_hit_count + m_miss_count;
    }

    // Print information on the cache (hit rate, etc.)
    // Prints nothing if the cache was never used
    void print_cache_data(ostream &out) const;

    // Prints information for all the registered caches (in
    // all_caches)
    static void print_all_cache_data(ostream &out);
};

