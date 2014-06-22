// constraint-cache.c
//
// A constraint cache that contains symbolic constraints from the constraint
// library. This can be used by any checker whose state contains symbolic
// constraints
//


//
// 
//

#include "constraint-cache.hpp"
#include "debug/output.hpp"                         // GDOUT
#include "sm/state/compose-nstate.hpp"
#include "sm/checker.hpp"                           // a_checker_pass_t
#include "sm/compose-nsm.hpp"                       // compose_nsm_t
#include "ast/cfg.hpp" // cfg_edge_t::is_backedge()
#include "traversal/abstract-interp.hpp" // get_edge_list
#include "sm/state/constraint-store.hpp"         // constraint_store_t
#include "constraint/constraint.hpp"             // c_factory
#include "constraint-utils/backtrack-stack.hpp"  // bt_stack_interface
#include "constraint-utils/constraint-utils.hpp" // is_same_constraint
#include "time/activity-path-timer.hpp"                  // DEF_ACTIVITY
#include "analysis/traversal/arenas.hpp"         // g_current_fn_arena
#include "analysis/work-unit/work-unit.hpp"      // constraintFPPEnablement

// For use with analysis/parseargs.cpp.
namespace debug {
    bool constraint_cache = false;
}

constraint_cache_entry_t::
constraint_cache_entry_t(arena_t *ar,
                         c_expr_c_expr_map_t *latest_var_uf_idx_map,
                         hash_map<const char *, unsigned, hash_cstr, eqstr> *uf_last_idx_map,
                         const VectorBase<c_expr *> &constraints_list,
                         loop_iter_t& li)
    : latest_var_uf_idx(latest_var_uf_idx_map),
      uf_last_idx(uf_last_idx_map),
      constraints(ar),
      loop_iter(ar) {

    // We don't need to copy the two maps, since the maps are pointers to
    // maps inside backtrack stack nodes within bt_stack_interface. These nodes
    // do not get destroyed until the end of a function.

    // Copy the list of constraints (pointers), though
    constraints.reserve(constraints_list.size());
    foreach(it, constraints_list) {
        constraints.push_back(*it);
    }

    // Copy the loop iteration vector
    if (li.is_loop()) {
        foreach(it, li.iters) {
            loop_iter.iters.push_back(*it);
        }
    }
}

//======================== constraint_cache_t ==============================

constraint_cache_t::constraint_cache_t(arena_t *ar):
    cache_entries(ar),
    seen_iters(ar),
    loop_iter_checker_state_map(ar),
    num_misses(0) { }

constraint_cache_t::~constraint_cache_t() {
}

// constraint_cache_t::add_cache_entry
//
// Update the cache with the entry
void
constraint_cache_t::add_cache_entry(c_expr_c_expr_map_t *latest_var_uf_idx_map,
                                    hash_map<const char *, unsigned, hash_cstr, eqstr> *uf_last_idx,
                                    const VectorBase<c_expr *> &constraints,
                                    abstract_interp_t &cur_traversal,
                                    loop_iter_t& li) {
    // First check if this set of constraints already exists as a cache entry.
    // This can be really painful to do taking the latest indices into
    // consideration. So, we do a crude approximation by simply checking for
    // equality of the lists. This is sound, but not complete.
    // Why sound? The constraints in the cache will always reference the latest
    // index of a var or a uf. Hence, exact equality means that the entries
    // actually are the same
    foreach(ce_it, cache_entries) {
        // We check for straight list equality, which assumes that the lists
        // are in the same order
        const VectorBase<c_expr *> &ce_clist =
            (*ce_it)->get_constraints();
        if (ce_clist == constraints && (*ce_it)->loop_iter == li) {
            return;
        }
    }
    constraint_cache_entry_t *cache_entry =
        new (cur_traversal.get_cache_arena()) constraint_cache_entry_t(
            cur_traversal.get_cache_arena(),
            latest_var_uf_idx_map,
            uf_last_idx,
            constraints,
            li);
    cache_entries.push_back(cache_entry);
}

// constraint_cache_t::clear_cache()
//
// Clears cache entries
void constraint_cache_t::clear_cache() {
    cache_entries.resize(0);
    seen_iters.clear();
    loop_iter_checker_state_map.clear();
    num_misses = 0;
}

// Recursive function that does an exhaustive search of the cache vs incoming
// The call to \p is_cache_entry_subset_of_incoming matches all the cache
// entries starting \p ccm_iter.
// \p entry_idx_map is used to transfer the map from cache var/uf indices to
// incoming store var/uf indices across recursive calls
// We propagate a forward and reverse map of indicies from vars/ufs's in cache
// to vars/uf's in store
static bool
is_cache_entry_subset_of_incoming(c_factory *cf,
                                  cexpr_cexprlist_t & candidate_cache_matches,
                                  cexpr_cexprlist_t::iterator ccm_iter,
                                  name_idxmap_map_t& entry_var_idx_map,
                                  name_idxmap_map_t& entry_uf_idx_map,
                                  name_idxmap_map_t& entry_var_rev_idx_map,
                                  name_idxmap_map_t& entry_uf_rev_idx_map) {
    cexpr_cexprlist_t::iterator ccm_iter_copy = ccm_iter;

    foreach(listIt, ccm_iter->second) {
        // Clone the idx maps
        name_idxmap_map_t var_idx_map = entry_var_idx_map;
        name_idxmap_map_t uf_idx_map = entry_uf_idx_map;
        name_idxmap_map_t var_reverse_idx_map = entry_var_rev_idx_map;
        name_idxmap_map_t uf_reverse_idx_map = entry_uf_rev_idx_map;
        ccm_iter = ccm_iter_copy;

        // Match the cache constraint to the candidate in the list that we
        // are currently handling.
        // If there is no match, go to the next one
        if (!is_same_constraint(cf, ccm_iter->first, *listIt, var_idx_map,
                                uf_idx_map, var_reverse_idx_map,
                                uf_reverse_idx_map)) {
            continue;
        }

        // Increment the candidate_cache_matches iterator, ccm_iter
        ++ccm_iter;

        // Recursive call to try out the entries starting the incremented
        // ccm_iter
        if (ccm_iter != candidate_cache_matches.end()) {
            if (is_cache_entry_subset_of_incoming(cf, candidate_cache_matches,
                                                  ccm_iter, var_idx_map,
                                                  uf_idx_map,
                                                  var_reverse_idx_map,
                                                  uf_reverse_idx_map)) {
                return true;
            }
        } else {
            // We have found a match for the final cache constraint.
            // Its a success!
            return true;
        }
    }
    return false;
}

// The core routine that checks if the set of constraints in the cache is
// a subset of the constraints in the incoming store called by check_cache_hit
// in constraint-cache.
// For loop edges, we distinguish between cache entries in different
// iterations, due to which the loop_iter is passed in.
// DEFAULT BEHAVIOR: constraint-cache requires ALL cache entries to match
// the constraints in the incoming store for a hit.  This would mean that we
// have a cache miss even if one cache entry in a constraint cache does not
// match the incoming constraints in the store
// NB:The flag "enable_or_constraint_caching" enables constraint caching where
// only one cache entry needs to match the constraints in the incoming store
// for a hit. This makes caching unsound, but faster since there are more hits.
static bool is_cache_hit_helper(state_t *sm_st,
                                constraint_cache_t *cache,
                                abstract_interp_t &cur_traversal,
                                loop_iter_t& loop_iter) {
    const arena_vector_t<constraint_cache_entry_t *> &cache_entries =
        cache->get_entries();
    GDOUT(constraint_cache, "In is_cache_hit_helper");

    if (cache_entries.size() == 0) {
        GDOUT(constraint_cache, "cache hit");
        return true;
    }

    constraint_store_t* store = dynamic_cast<constraint_store_t*>(sm_st);
    ostr_assert(store,
                "constraint_cache_t only works with constraint stores"
                << endl);

    c_factory *cf = store->get_cf();
    ostr_assert(cf,
                "c_factory in constraint store needs to be set for cache hit checking");

    list<c_expr *> incoming;
    store->get_constraint_list(incoming);
    cexpr_store_t &incoming_cexpr_map = store->get_cstore();
    uf_last_idx_store_t &incoming_uf_idx_map = store->get_uf_idx_store();

    // Generate a list of incoming cexpr's corresponding to each zero-index
    // cexpr.
    cexpr_cexprlist_t incoming_zero_idx_map;
    GDOUT(constraint_cache, "[Incoming constraints] : " <<
          incoming.size() << endl);
    foreach(incomingIt, incoming) {
        c_expr *zero_idx_incoming = generate_zero_idx(cf, *incomingIt);
        incoming_zero_idx_map[zero_idx_incoming].push_back(*incomingIt);
        if (debug::constraint_cache) {
            cout << *incomingIt << endl;   // Print the constraint
        }
    }
    if(debug::constraint_cache) {
        if (loop_iter.is_loop()) {
            GDOUT(constraint_cache, "Loop iteration: " << loop_iter << endl);
        }
        GDOUT(constraint_cache, "Incoming zero idx map has "
              << incoming_zero_idx_map.size() << " entries" << endl);
    }

    bool found_hit = false;
    bool found_miss = false;

    // Get the corresponding state machine. We use this, if necessary for
    // backpatching
    sm_t *sm = sm_st->sm;
    bt_stack_interface *sm_with_bt_stack =
        dynamic_cast<bt_stack_interface *>(sm);

    ostr_assert(sm_with_bt_stack,
                "constraint_cache_t only works with checkers that have a backtrack stack");

    foreach(cache_entry_it, cache_entries) {
        if ((*cache_entry_it)->constraints.size() == 0) {
            // This should never happen in reality. Assert?
            continue;
        }

        if (loop_iter.is_loop() &&
            (*cache_entry_it)->loop_iter != loop_iter
            // NB: The condition commented below causes a BIG performance hit!
            // (For iterations later than the threshold, check
            // it against every one of them)
            //            &&
            //            !is_later_loop_iter(loop_iter, 2)
        ) {
            // We are dealing with an edge in a loop. Check that the cache
            // entry's loop iteration matches the loop_iter of the edge
            // in the current traversal. Otherwise, the cache entry is
            // not applicable.
            GDOUT(constraint_cache, "Skipped entry with loop iteration: "
                  << (*cache_entry_it)->loop_iter);
            continue;
        }

        GDOUT(constraint_cache, "[constraints in cache entry]");
        if (debug::constraint_cache) {
            int cons_idx = 0;
            c_print_cache_t pcache;
            foreach(consIt, (*cache_entry_it)->constraints) {
                cout << "[" << cons_idx++ << "]" << endl;
                (*consIt)->print(cout, pcache, /*result*/ 0);
                cout << endl;
            }
        }

        c_expr_c_expr_map_t& cache_latest_var_uf_idx =
            *(*cache_entry_it)->latest_var_uf_idx;
        hash_map<const char *, unsigned, hash_cstr, eqstr>& cache_uf_last_idx =
            *(*cache_entry_it)->uf_last_idx;

        // The cached constraints have already been filtered, while they were
        // being inserted to include only those constraints that have some
        // effect on the latest values of vars/uf's at this point

        // Map the constraints in the cache and incoming to constraints with
        // index 0 throughout
        // Create map from constraints in cache to candidate constraints in
        // incoming
        // If any constraint in the cache matches no constraints in incoming
        // (with index 0, remember), then it clearly can't be a subset and we
        // simply return false
        c_expr_set_t cache_with_zero_idx;

        bool zero_idx_matches = true;

        cexpr_cexprlist_t candidate_cache_matches;

        // We will subsequently use candidate_cache_matches to do an exhaustive
        // search for a match. This is computationally daunting. So, we want
        // to track the total number of matches that are possible, so we can
        // give up if it exceeds a threshold.
        unsigned long long potential_matches = 1;

        foreach(constraintIt, (*cache_entry_it)->constraints) {
            c_expr *zero_idx_cexpr = generate_zero_idx(cf, *constraintIt);
            cexpr_cexprlist_t:: iterator izimIt =
                incoming_zero_idx_map.find(zero_idx_cexpr);
            if (izimIt == incoming_zero_idx_map.end()) {
                // If an all-zero-idx cexpr is not found in incoming,
                // that exists in the cache then this is a cache miss
                zero_idx_matches = false;
                break;
            } else {
                // Otherwise, the constraint in the cache is mapped to the set
                // of candidate matches in the incoming set
                candidate_cache_matches[*constraintIt] = izimIt->second;
                // Trick to avoid integer overflow. The max value is now 10000.
                // So, this works for now.
                if (izimIt->second.size() > (unsigned) max_constraint_cache_matches) {
                    potential_matches = max_constraint_cache_matches + 1;
                    break;
                }
                potential_matches *= izimIt->second.size();
                if (potential_matches == 0 ||
                    potential_matches > (unsigned) max_constraint_cache_matches) {
                    break;
                }
            }
        }

        // The zero indices did not match. Try the next cache entry
        if (!zero_idx_matches || potential_matches == 0) {
            found_miss = true;
            // If or-constraint caching is NOT enabled, then, we found a miss
            // in a cache entry, which makes this a cache miss
            if (!enable_or_constraint_caching) {
                break;
            }
            continue;
        }

        // If the number of potential matches we will need to run an
        // exhaustive search on, exceeds a threshold, don't attempt it.
        // We need this, even though we have already bounded the number of
        // cache enties itself using max_constraints_in_cache elsewhere
        // Call it a hit.
        if (potential_matches > (unsigned) max_constraint_cache_matches) {
            found_hit = true;
            // If or-constraint caching is enabled, then, we found a hit
            // in a cache entry, which makes this a cache hit
            if (enable_or_constraint_caching) {
                break;
            }
            continue;
        }

        // Implement an exhaustive search of the potential matchings between
        // the cache constraints and the constraints in incoming

        // This is a classic backtracking algorithm that uses recursion to
        // explore all the combinations of matches from candidate_cache_matches

        // Initialize var_idx_map/uf_idx_map with a mapping from the latest
        // indices for vars/uf's in the cache to latest indices for vars/uf's
        // in the incoming store
        // We keep a forward and a reverse map from indices of vars/uf's in the
        // cache to indices of the same vars/uf's in the store
        name_idxmap_map_t var_idx_map, uf_idx_map, var_reverse_idx_map,
            uf_reverse_idx_map;

        // The intial mapping from latest index for a var in cache to the
        // latest index for the same var in the incoming store
        foreach(cacheIt, cache_latest_var_uf_idx) {
            // Get the latest index of the same key in the incoming store
            // and map the cache idx to the incoming idx
            unsigned idx_in_incoming;
            if (c_var *cvar = dynamic_cast<c_var *>(cacheIt->second)) {
                LET(incomingIt, incoming_cexpr_map.find(cacheIt->first));
                if (incomingIt == incoming_cexpr_map.end()) {
                    idx_in_incoming = 0;
                } else {
                    c_var *incoming_cvar =
                        dynamic_cast<c_var *>(incomingIt->second);
                    ostr_assert(incoming_cvar,
                                "c_var not mapped to c_var in idx store?");
                    idx_in_incoming = incoming_cvar->get_idx();
                }

                // Now map the latest index for a var in the cache to the
                // latest index for the var in the incoming store
                var_idx_map[cvar->get_name()][cvar->get_idx()] =
                    idx_in_incoming;
                var_reverse_idx_map[cvar->get_name()][idx_in_incoming] =
                    cvar->get_idx();
            }

            // Do nothing for uninterpreted function entries here
        }

        // The initial mapping from latest idx for uf's with a certain name in
        // cache to latest idx for uf with same name in the incoming store
        foreach(cacheIt, cache_uf_last_idx) {
            LET(incomingIt, incoming_uf_idx_map.find(cacheIt->first));
            if (incomingIt != incoming_uf_idx_map.end()) {
                uf_idx_map[cacheIt->first][cacheIt->second] =
                    incomingIt->second;
                uf_reverse_idx_map[cacheIt->first][incomingIt->second] =
                    cacheIt->second;
            } else {
                uf_idx_map[cacheIt->first][cacheIt->second] = 0;
                uf_reverse_idx_map[cacheIt->first][0] = cacheIt->second;
            }
        }

        // Call to is_cache_entry_subset_of_incoming (a recursive function for
        // exhaustive search)
        if (is_cache_entry_subset_of_incoming(cf, candidate_cache_matches,
                                              candidate_cache_matches.begin(),
                                              var_idx_map, uf_idx_map,
                                              var_reverse_idx_map,
                                              uf_reverse_idx_map)) {
            found_hit = true;
            // When we find a hit, we need to backpatch the entry that caused
            // the hit, because, we may need to traverse a fraction of the
            // path that led to the hit again, if we have different constraints
            // We call this cache inheritance.
            const VectorBase<c_expr *> cache_clist =
                (*cache_entry_it)->get_constraints();
            const VectorBase<c_expr *> &incoming_clist =
                store->get_constraint_vector();
            sm_with_bt_stack->backpatch_on_cache_hit(incoming_clist,
                                                     cache_clist,
                                                     store, cur_traversal);

            // If or-constraint caching is enabled, then, we found a hit
            // in a cache entry, which makes this a cache hit
            if (enable_or_constraint_caching) {
                break;
            }
        } else {
            found_miss = true;
            // If or-constraint caching is not enabled, then, we found a miss
            // in a cache entry, which makes this a cache miss
            if (!enable_or_constraint_caching) {
                break;
            }
        }
    }

    if (enable_or_constraint_caching) {
        if (found_hit) {
            GDOUT(constraint_cache, "cache hit  -- found a hitting cache entry"
            );
        } else {
            GDOUT(constraint_cache, "cache miss  -- no hits");
        }

        return found_hit;
    } else {
        // DEFAULT BEHAVIOUR (enable_or_constraint_caching is false)
        if (!found_miss) {
            GDOUT(constraint_cache, "cache hit  -- found no misses in cache entries"
            );
        } else {
            GDOUT(constraint_cache, "cache miss  -- no hits");
        }
        return !found_miss;
    }

    // Silence compiler warning
    return found_hit;
}

// Call only for a loop edge
// Returns false if the checker state causes a cache miss on a loop edge
// Returns true otherwise
bool constraint_cache_t::check_checker_hit_helper(sm_t* sm,
                                                  abstract_interp_t &cur_traversal,
                                                  loop_iter_t& loop_iter) {
    if (loop_iter.iters.empty()) {
        return true;
    }
    cond_assert(str_equal(sm->get_sm_name(), "CONSTRAINT_FPP"));
    sm_t *fpp_sm = sm->get_parent();
    string_assert(fpp_sm != NULL, "Should be in FPP composition");
    sm_t *overall_composition = fpp_sm->get_parent();
    string_assert(overall_composition != NULL,
                  "FPP composition should itself be in a composition");
    // Its parent is fpp sm, whose parent is a compose sm that
    // contains the corresponding a_checker_pass_t
    // TODO: What about checkers like integer overflow and buffer overflow
    // Checker caching is flawed there too. Make this happen.

    compose_nsm_t *compose_sm =
        dynamic_cast<compose_nsm_t *>(overall_composition);

    cond_assert(compose_sm != NULL);

    {
        const vector<sm_t *> &sm_list = compose_sm->get_fn_sms();
        foreach (i, sm_list) {
            sm_t *sm2 = *i;
            if (a_checker_pass_t *c2pass_sm =
                dynamic_cast<a_checker_pass_t *>(sm2)) {
                // We have found the corresponding checker composition
                GDOUT(constraint_cache,
                      "We are in a checker "
                      << c2pass_sm->get_sm_name());
                // Now, check the checker's state

                // Get the state
                state_t *checker_state =
                    cur_traversal.current_state(c2pass_sm);
                // We have a pass2 composition state
                if (compose_nstate_t *compose_nstate =
                    dynamic_cast<compose_nstate_t *>(checker_state)) {
                    // Find the pass2 checker within it
                    for(unsigned j = 0;
                        j < compose_nstate->get_state_num(); j++) {
                        string sm_j_name;
                        if (compose_nstate->get_nth_state(j)->sm->get_sm_name()) {
                            sm_j_name =
                                compose_nstate->get_nth_state(j)->sm->get_sm_name();
                        }
                        if (sm_j_name.find("_pass2") != string::npos) {
                            checker_state = compose_nstate->get_nth_state(j);
                            break;
                        }
                    }
                }
                // If the checker state is a composed store, keep
                // getting the first state in the composition
                // (that is the checker state we seek)
                while (compose_nstate_t *compose_nstate =
                       dynamic_cast<compose_nstate_t *>(checker_state)) {
                    checker_state = compose_nstate->get_nth_state(0);
                }
                if (subset_state_t *checker_subset_state =
                    dynamic_cast<subset_state_t *>(checker_state)) {
                    // The checker uses subset state. We don't care
                    // about other kinds of checkers

                    // Convert State to a get a mapping to size_t
                    stack_allocated_arena_t ar("subset constraint cache");
                    subset_cache_type::subset_hash_t current_state(&ar);
                    {
                        START_STOP_ACTIVITY(sm->hash_timer());
                        // Add a sub-timer with constant name for
                        // later aggregation
                        DEF_ACTIVITY("Constraint cache Hashing");
                        checker_subset_state->compute_hash(current_state);
                    }

                    subset_cache_type::cache_pair_set_t *&this_loop_iter =
                        loop_iter_checker_state_map[loop_iter];
                    if(!this_loop_iter) {
                        this_loop_iter =
                            subset_cache_pair_set_t::create(cur_traversal.get_cache_arena());
                    }

                    foreach(it, current_state) {
                        bool found_new = false;
                        // Update the state, if required
                        if (this_loop_iter->insert(*it).second) {
                            found_new = true;
                        }
                        if (found_new) {
                            return false;
                        }
                    }

                }

                break;
            }
        }
    }
    return true;
}

bool constraint_cache_t::check_cache_hit_helper(state_t* sm_st,
                                                const cfg_edge_t *edge,
                                                abstract_interp_t &cur_traversal) {
    DEF_ACTIVITY("constraint cache hit");
    if (!edge) {
        // If there is no edge, return a miss. This can happen in one case
        // after function calls
        return false;
    }

    // Update the cache pointer in the backtrack stack maintained by
    // the bt stack in the state machine
    sm_t *sm = sm_st->sm;
    bt_stack_interface *sm_with_bt_stack =
        dynamic_cast<bt_stack_interface *>(sm);

    ostr_assert(sm_with_bt_stack,
                "constraint_cache_t only works with checkers that have a backtrack stack");
    sm_with_bt_stack->add_cache_to_backtrack_stack(this, edge);

    GDOUT(constraint_cache, "Checking cache hit at "
          << *edge << " with cache");

    constraint_store_t *store = dynamic_cast<constraint_store_t*>(sm_st);
    ostr_assert(store,
                "constraint_cache_t only works with constraint stores"
                << endl);
    cfg_t *cfg = cur_traversal.get_cfg();

    // If this is a loop edge, execute the following algorithm
    // (i) Check if the iteration number is > 2 in any of the iterations.
    //     If it is, then fall through to the regular cache check, but with
    //     the current loop iteration.
    // (ii) Check if we've never seen this iteration on this edge before. If
    //      we haven't, then it is a cache miss, otherwise we do the regular
    //      cache check with the current loop iteration
    loop_iter_t loop_iter(g_current_fn_arena());
    if (cfg->is_loop_edge(edge)) {
        GDOUT(constraint_cache, "Loop edge found");

        // Initialize loop_iter for edge, 'edge'
        sm_with_bt_stack->get_latest_loop_iter(edge, cfg, loop_iter);

        bool iter_beyond_unroll_threshold = false;
        foreach(lit, loop_iter.iters) {
            if (lit->iter_id > 2) {
                iter_beyond_unroll_threshold = true;
                break;
            }
        }

        if (!iter_beyond_unroll_threshold) {
            // Check the corresponding checker state (if one exists) and see if
            // we have seen that checker state at this loop iteration
            // Do this before we check seen_iters so that we can update the
            // checker state seen in this loop iteration appropriately
            if (cur_traversal.get_wu().constraintFPPEnablement == CFPP_CONSTRAINT_FPP_ONLY
                &&
                !check_checker_hit_helper(sm, cur_traversal, loop_iter)) {
                // Update seen_iters before returning
                seen_iters.insert(loop_iter);
                GDOUT(constraint_cache,
                      "At this constraint cache with a different checker state"
                );
                return false;
            }

            if (seen_iters.find(loop_iter) == seen_iters.end()) {
                GDOUT(constraint_cache,
                      "This iteration " << loop_iter
                      << " not seen earlier. Declaring it a miss");
                seen_iters.insert(loop_iter);
                return false;
            }
        }
        seen_iters.insert(loop_iter);

    }

    // For non-loop edges and the case that falls through the loop edge
    // algorithm above, execute the following algorithm
    if (cache_entries.size() == 0) {
        GDOUT(constraint_cache, "Empty cache found");
        return true;
    }

    // We have a cap on the maximum number of cache misses for a particular
    // edge in the CFG
    if (num_misses > max_constraint_cache_misses) {
        GDOUT(constraint_cache, "Exceeded miss threshold for cache");
        return true;
    }

    bool is_hit = is_cache_hit_helper(sm_st, this, cur_traversal, loop_iter);
    if (!is_hit) {
        num_misses++;
    }

    return is_hit;
}

// constraint_cache_t::check_cache_hit
//
// Check for a hit on the cache. This means that the set of constraints in the
// cache is a subset of the set of constraints in the store
bool constraint_cache_t::check_cache_hit
(state_t* state,
 abstract_interp_t &cur_traversal,
 const cfg_edge_t *edge,
 bool widen,
 bool first_time)
{
    if (debug::caching || debug::constraint_cache) {
        cout << endl << "Constraint cache" << endl;
    }
    bool rv = check_cache_hit_helper(state, edge, cur_traversal);
    if (debug::caching || debug::constraint_cache) {
        cout << "result: " << (rv ? "HIT" : "MISS") << endl;
    }
    return rv;
}
