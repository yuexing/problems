// hash function ideas

// implement a hashmap
template<class T>
struct hash_t 
{
    int operator() (const T& i);
};

template<>
struct hash_t<int> 
{
    int operator() (const int& i)
    {
        return i;
    }
};

template<class T>
struct hash_t<T*>
{
    int operator() (const T*& i)
    {
        return i;
    }
};

template<class T>
struct compare_t
{
    bool operator() (const T& i, const T& j);
};

template<class K, class V, class Hash = hash_t<K>, class Compare = compare_t<K> >
struct hash_map_t
{
    struct pair_t
    {
        K k;
        V v;

        pair_t(K k, V v): k(k), v(v) {}
    };

    typedef K       key_type;
    typedef V       mapped_type;
    typedef pair_t  value_type;
    typedef Compare key_compare;

    struct hash_node_t
    {
        pair_t p;
        hash_node_t *next;

        hash_node_t(K k, V v): p(k, v) {}
    };

    struct iterator_t
    {
        friend class hash_map_t;

        iterator_t(hash_map_t &map, int idx, hash_node_t *n)
          : map(map), idx(idx), n(n){
            next_avail();
          }

        iterator_t &operator++()
        {
            n = n->next;
            next_avail();
            return *this;
        }

        void next_avail()
        {
            while(!n && idx < map.table_size) {
                ++idx;
                n = map.table[idx];
            }
        }

        bool operator==(const iterator_t &o)
        {
            return &map == &o.map && o.idx == idx && o.n == n;
        }

        hash_node_t &operator*()
        {
            return *(n->p);
        }

    private:
        hash_map_t &map;
        int idx;
        hash_node_t *n;
    };

    hash_map_t(const Compare& compare = Compare(),
               const Hash& hash = Hash()) {}

    // NB: put and get can both implemented with sorted list,
    // then the put/get can be log(l).
    void put(const K &k, const V &v)
    {
        int idx = get_idx();
        hash_node_t *n = new hash_node_t(k, v);
        n->next = table[idx];
        table[idx] = n;
    }

    const V& get(const K &k)
    {
        iterator_t it = find(k);
        if(it != end()) {
            return *it;
        }
    }

    iterator_t find(const K &k)
    {
        int idx = get_idx(k);
        hash_node_t *n = table[idx];
        while(n) {
            if(!compare(n->k, k)) {
                return iterator_t(*this, idx, n); 
            }
            n = n->next;
        }
    }

    iterator_t begin()
    {
        return iterator_t(*this, 0, table[0], table);
    }

    iterator_t end()
    {
        return iterator_t(*this, table_size, 0);
    }
private:
    int get_idx(const K &k) 
    {
        return hash(k) % table_size; 
    }
    hash_node_t **table;
    int table_size;
    Hash hash;
    Compare compare;
};
