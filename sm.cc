struct state_t;

struct node_t
{
    node_t *const next;
};

struct cache_t
{
    bool check_cache_hit(const state_t* s, bool first_time) ;
};

struct composed_ncache_t : public cache_t
{
    void add(cache_t *);
    cache_t *nth(int n);

    bool check_cache_hit(const state_t* s);    
};

struct state_t
{
    cache_t *create_empty_cache();    

    // always widen in caching
    bool widens();
    void widen(cache_t *);
};

#if 0
bool cache_t::check_cache_hit(const state_t* s, bool first_time) 
{
    // widen
    if(s->widens()) {
        s->widen(this);
    }
    // check hash hit based on size_t
}
#endif

struct composed_nstate_t : public state_t
{
    void add(state_t *);
    state_t *nth(int n);

    cache_t *create_empty_cache();    

    bool widens();
    void widen(cache_t *);
};

class sm_t
{
    state_t *create_empty_state();
    void handle(const node_t *n, state_t *s);
};

// manipulate statistics
class stat_user_t
{
    // store the stat about a function
    void storeStat();
    // aggregate the stat from all functions together
    void computeInferredStat();
    // use the stat
    void getStat();
};

struct stat_sm_t : public sm_t
{
    stat_user_t stat_user;
}

struct composed_nsm_t : public sm_t
{
    void add(sm_t *);
    sm_t *nth(int n);
    int idx(sm_t *);

    state_t *create_empty_state();
    void handle(const node_t *n, state_t *s);
};

class abstract_interp
{
    void add(sm_t *sm);
    state_t *get_state(sm_t *sm);

    void run(const node_t *n) {
        bool first_time = true;
        state_t *state = nsm.create_empty_state();
        cache_t *cache = state->create_empty_cache();
        // state/cache are cloned on each branch
        while(n) {
           nsm.handle(n, state); 
           n = n->next;
           if(first_time) {
               first_time = false;
           }
           if(cache->check_cache_hit(state, first_time)) {
               break;
           }
        }
    }
private:
    composed_nsm_t nsm;
};
