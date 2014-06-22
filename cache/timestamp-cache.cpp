/**
 * \file timestamp-cache.cpp
 *
 * Since the cache structure is a template, this doesn't include the
 * actual implementation, but only the unit tests.
 *
 **/

using std::deque;

cache_base_t::cache_base_t
(const char *name,
 size_t max_size):
    m_name(name),
    m_max_size(max_size),
    m_hit_count(0),
    m_miss_count(0) {
    if(!all_caches) {
        all_caches = new vector<cache_base_t *>;
    }
    all_caches->push_back(this);
}

cache_base_t::~cache_base_t() {
    // Remove from vector.
    // Search backwards to optimize LIFO
    // See similar code in ~arena_t
    vector<cache_base_t *> &all = *all_caches;
    LET(i, std::find(all.rbegin(), all.rend(), this));
    ostr_assert(i != all.rend(),
                "Cache " << m_name << " not found in global vector");
    // base has an offset of 1 from the actual location we're
    // interested in, e.g. rbegin().base() == end()
    all.erase(i.base() - 1);
}

vector<cache_base_t *> *cache_base_t::all_caches;

double cache_base_t::hit_rate() const {
    if(!call_count()) {
        return 0;
    } else {
        return (double)m_hit_count / (double)call_count();
    }
}

double cache_base_t::miss_rate() const {
    if(!call_count()) {
        return 0;
    } else {
        return (double)m_miss_count / (double)call_count();
    }
}

void cache_base_t::print_cache_data(ostream &out) const {
    if(!call_count()) {
        return;
    }
    out << m_name << " cache miss rate: "
        << percentage(m_miss_count, call_count(), /*includeValues*/true)
        << endl;
}

void cache_base_t::print_all_cache_data(ostream &out) {
    if(!all_caches)
        return;
    foreach(i, *all_caches) {
        (*i)->print_cache_data(out);
    }
}

// Explicitly instantiate one version, to make sure this all compiles.
template class timestamp_cache_t<int, int>;
template class timestamp_hash_cache_t<int, int>;

OPEN_ANONYMOUS_NAMESPACE;

typedef timestamp_cache_t<int, int> test_cache_base_t;

struct test_cache_t:
    public test_cache_base_t  {
    test_cache_t(int max_size):
        test_cache_base_t("Cache", max_size) {
    }

    void inner_get_data(const int &key,
                        int &data) {
        data = key;
    }

    void check_cache_size(int size) {
        cond_assert(this->size() == size);
    }
};

CLOSE_ANONYMOUS_NAMESPACE;

static void test_cache
(test_cache_t &cache,
 int test_half_size) {
    deque<int> last_elements;
    for(int i = 0; i < test_half_size * 20; ++i) {
        // Choose a random element in [0; test_half_size * 5]
        // We want collisions but not too many, this should do.
        int index = rand() % (test_half_size * 5);
        int n = cache.get_data(index);
        // Make sure we got the right value
        cond_assert(n == index);
        // Make sure the cache didn't get too big
        cond_assert(cache.size() < test_half_size * 2);
        // Make sure the most recent (1/2-size) elements are in cache.
        last_elements.push_back(index);
        if(last_elements.size() > test_half_size) {
            last_elements.pop_front();
        }
        foreach(j, last_elements) {
            cond_assert(contains(cache.cache_map(), *j));
        }
    }
}

OPEN_ANONYMOUS_NAMESPACE;

// The goal of this structure is to make it impossible to compare with
// '<'
struct int_wrapper_t {
    int_wrapper_t(int i):i(i) {
        
    }
    int i;
};
CLOSE_ANONYMOUS_NAMESPACE;

static bool comp(const int_wrapper_t &i1, const int_wrapper_t &i2) {
    return i1.i < i2.i;
}

void test_replace_heap_top() {
    vector<int_wrapper_t> vect;
    for(int i = 0; i < 10; ++i) {
        vect.push_back(rand() % 20);
        std::push_heap(vect.begin(), vect.end(), comp);

        int_wrapper_t new_val(rand() % 20);
        vect.front() = new_val;
        replace_heap_top(vect.begin(), vect.end(), comp);
        // __is_heap is a gcc extension.
        // This used to check that the resulting vector was identical
        // to the result of pop_heap + push_heap but it doesn't have
        // to be the case.
        cond_assert(std::__is_heap(vect.begin(), vect.end(), comp));
    }
}

// Check that 2 floats are sufficiently close (within 1%)
// Using == on floats is very unreliable, and we don't need a lot of
// precision for this check.
static void assertCloseFloats(double d1, double d2)
{
    double ratio = d1 / d2;
    cond_assert(0.99 < ratio && ratio < 1.01);
}

void timestamp_cache_unit_tests() {

    test_replace_heap_top();
    
    int test_half_size = 20;
    test_cache_t cache1(test_half_size * 2);
    test_cache(cache1, test_half_size);
    // A second time, see what happens when the cache doesn't start empty.
    test_cache(cache1, test_half_size);
    test_half_size = 8;
    cache1.resize(test_half_size * 2);
    cond_assert(cache1.size() < test_half_size * 2);
    test_cache(cache1, test_half_size);
    // Test hit/miss rate.
    {
        test_cache_t cache(/*size*/2);
        cond_assert(cache.hit_rate() == 0);
        cond_assert(cache.miss_rate() == 0);
        bool hit;
#define CHECK_MISS(n)                                               \
        cache.get_data_ref(n, /*call_inner_get_data*/true, &hit);   \
        cond_assert(!hit)
#define CHECK_HIT(n)                                                \
        cache.get_data_ref(n, /*call_inner_get_data*/true, &hit);   \
        cond_assert(hit)
        CHECK_MISS(0);
        CHECK_HIT (0);
        CHECK_MISS(1);
        CHECK_HIT (1);
        CHECK_MISS(2);
        CHECK_MISS(0);
        assertCloseFloats(cache.hit_rate(), (double)2 / (double)6);
        assertCloseFloats(cache.miss_rate(), (double)4 / (double)6);
    }
}
