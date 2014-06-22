/**
 * \file compose-eq-cache.cpp
 *
 * 
**/
#include "compose-eq-cache.hpp"

#include "analysis/sm/state/compose-nstate.hpp" // compose_nstate_t

#include "libs/debug/output.hpp"          // BEGIN_IF_DEBUG
#include "libs/exceptions/assert.hpp"     // xfailureb
#include "libs/tutils.hpp" // compare
#include "libs/time/activity-path-timer.hpp" // DEF_ACTIVITY

static void decomposeState(
    const state_t *st,
    vector<subset_state_t *> &subStates)
{
    const compose_nstate_t *ns = static_cast<const compose_nstate_t *>(st);
    foreach(i, ns->get_states()) {
        state_t *s = *i;
        if(subset_state_t *ss = s->ifsubset_state_t()) {
            subStates.push_back(ss);
        } else if(s->is_empty_state()) {

        } else {
            xfailureb("State for " << (*i)->sm->get_sm_name()
                      << " is not subset or empty");
        }
    }
}

compose_eq_cache_t::compose_eq_cache_t(arena_t *ar):
    cachedStates(ar),
    widenInfo(NULL) {
}

compose_eq_cache_t::CachedStateComposition::CachedStateComposition(
    arena_t *ar, const VectorBase<subset_state_t *> &subStates) {
    states = arenaVectorAlloc<CachedState>(subStates.size(), *ar);
    stack_allocated_arena_t tmpAr("CachedStateComposition");
    for(int i = 0; i < subStates.size(); ++i) {
        subset_hash_t cs(&tmpAr);
        subStates[i]->compute_hash(cs);
        states[i] = arenaVectorCopy(cs, *ar);
    }
}

compose_eq_cache_t::CachedStateComposition::CachedStateComposition(
    arena_t *ar, const CachedStateComposition &other):
    states(arenaVectorCopy(other.states, *ar)) {
    foreach(i, states) {
        *i = arenaVectorCopy(*i, *ar);
    }
}

void compose_eq_cache_t::CachedStateComposition::write_as_text(
    IndentWriter &out,
    const VectorBase<subset_state_t *> &subStates) const
{
    stack_allocated_arena_t ar("CachedStateComposition::write_as_text");
    for(int i = 0; i < subStates.size(); ++i) {
        subset_state_t *s = subStates[i];
        out << "State for " << s->sm->get_sm_name() << '\n';
        out.indent();
        s->print_cache(out, states[i]);
        out << '\n';
        out.dedent();
    }
}


static int compareCachedStates(
    const compose_eq_cache_t::CachedState &s1,
    const compose_eq_cache_t::CachedState &s2)
{
    if(int cmp = s1.size() - s2.size()) {
        return cmp;
    }
    LET(i1, s1.begin());
    LET(i2, s2.begin());
    for(; i1 != s1.end(); ++i1, ++i2) {
        if(int cmp = expr_memory_compare(i1->first, i2->first)) {
            return cmp;
        }
        if(int cmp = ::compare(i1->second, i2->second)) {
            return cmp;
        }
    }
    return 0;
}

void compose_eq_cache_t::WidenInfo::add(
    const compose_eq_cache_t::CachedStateComposition &cachedState)
{
    for(int i = 0; i < statesThatWiden.size(); ++i) {
        const CachedState &s = cachedState.states[statesThatWiden[i]];
        subset_cache_pair_set_t &h = *widenCaches[i];
        foreach(i, s) {
            h.insert(*i);
        }
    }
}

int compose_eq_cache_t::CachedStateComposition::
    compare(const CachedStateComposition &other) const
{
    for(int i = 0; i < states.size(); ++i) {
        if(int cmp = compareCachedStates(states[i], other.states[i])) {
            return cmp;
        }
    }
    return 0;
}

void compose_eq_cache_t::WidenInfo::widen(
    const vector<subset_state_t *> &subStates)
{
    for(int i = 0; i < statesThatWiden.size(); ++i) {
        subStates[statesThatWiden[i]]->widen(*widenCaches[i]);
    }
}

compose_eq_cache_t::WidenInfo::WidenInfo(
    const vector<subset_state_t *> &subStates,
    const CachedStateSet &cachedStates)
{
    arena_t *ar = cachedStates.get_allocator().arena;
    vector<int> indices;
    for(int i = 0; i < subStates.size(); ++i) {
        if(subStates[i]->widens()) {
            indices.push_back(i);
        }
    }
    statesThatWiden = arenaVectorCopy<int>(indices, *ar);
    // Initialize.
    // Add all the existing states in "cachedStates" ("widen"
    // isn't necessarily true the first time this is called)
    widenCaches =
        arenaVectorAlloc<subset_cache_pair_set_t *>(statesThatWiden.size(), *ar);
    foreach(i, widenCaches) {
        *i = subset_cache_pair_set_t::create(ar);
    }
    foreach(i, cachedStates) {
        add(**i);
    }
}

bool compose_eq_cache_t::check_cache_hit(
    state_t *state,
    abstract_interp_t &cur_traversal,
    const cfg_edge_t *edge,
    bool widen,
    bool first_time)
{
    DEF_FREQUENT_ACTIVITY("compose_eq_cache_t::check_cache_hit");
    vector<subset_state_t *> subStates;
    decomposeState(state, subStates);
    arena_t *cacheArena = cachedStates.get_allocator().arena;

    if(widen) {
        DEF_FREQUENT_ACTIVITY("compose_eq_cache_t::widen");
        if(!widenInfo) {
            widenInfo = new (*cacheArena) WidenInfo(
                subStates,
                cachedStates);
        }
        widenInfo->widen(subStates);
    }
    static Activity tempStateConstruction("construct temp state");
    // Create a temporary state, for lookup.
    stack_allocated_arena_t ar("compose_eq_cache lookup");
    tempStateConstruction.start();
    CachedStateComposition lookupState(&ar, subStates);
    tempStateConstruction.stop();

    BEGIN_IF_DEBUG(caching);
    IndentWriter out(cout);
    out << "Cached states:\n";
    foreach(i, cachedStates) {
        (*i)->write_as_text(out, subStates);
    }
    out << "State being checked:\n";
    lookupState.write_as_text(out, subStates);
    END_IF_DEBUG(caching);

    if(contains(cachedStates, &lookupState)) {
        // Cache hit
        BEGIN_IF_DEBUG(caching);
        cout << "result: HIT" << endl;
        END_IF_DEBUG(caching);
        return true;
    }
    // Cache miss.
    // Add state to set. Make a permanent copy.
    CachedStateComposition *copyToInsert =
        new (*cacheArena) CachedStateComposition(
            cacheArena,
            lookupState
        );
    cachedStates.insert(copyToInsert);

    if(widenInfo) {
        // Add new state to widen caches if necessary.
        widenInfo->add(*copyToInsert);
    }

    BEGIN_IF_DEBUG(caching);
    cout << "result: MISS" << endl;
    END_IF_DEBUG(caching);
    return false;
}

compose_eq_state_t::compose_eq_state_t(
    arena_t *ar,
    sm_t *sm,
    const VectorBase<state_t *> &states):
    compose_nstate_t(ar, sm, states)
{
}

compose_eq_state_t::compose_eq_state_t(
    arena_t *ar,
    const compose_eq_state_t &other):
    compose_nstate_t(ar, other) {
    // Nothing special to do here, we just want the vtbl
}

state_t *compose_eq_state_t::clone(arena_t *ar) const {
    return new (*ar) compose_eq_state_t(ar, *this);
}

cache_t *compose_eq_state_t::
create_empty_cache(arena_t *ar) const
{
    return new (*ar) compose_eq_cache_t(ar);
}
