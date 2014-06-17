// (c) 2008-2010,2012-2014 Coverity, Inc. All rights reserved worldwide.
#include "constraint-store.hpp"

#include <queue>

using std::deque;

constraint_event_t::constraint_event_t(
    arena_t *a,
    const char *t,
    string const &event,
    bool needs_vals,
    const E_model *m):
    tag(global_string_manager->get_cstr(t)),
    description(
        g_evil_global_cur_abstract_interp->
        event_manager.get_unique_string(event.c_str())),
    needs_values(needs_vals),
    call_model(m),
    operands(a) {
}

constraint_event_t::constraint_event_t(
        const constraint_event_t &other,
        arena_t *a):
    tag(other.tag),
    description(other.description),
    needs_values(other.needs_values),
    call_model(other.call_model),
    operands(other.operands, a) {
}

constraint_event_t *constraint_event_t::clone(arena_t *ar) const
{
    return new (*ar) constraint_event_t(*this, ar);
}

cexpr_idx_set_t *clone_cexpr_idx_set(
    const cexpr_idx_set_t *s,
    arena_t *a)
{
    if(!s) {
        return NULL;
    }
    return s->clone(a);
}

sub_cexpr_event_map_t *clone_sub_cexpr_event_map(
    const sub_cexpr_event_map_t *s,
    arena_t *a){
    if(!s) {
        return NULL;
    }
    sub_cexpr_event_map_t *rv = s->clone(a);
    foreach(i, *rv) {
        i->second = i->second->clone(a);
    }
    return rv;
}


constraint_store_t::constraint_store_t(mc_arena a, sm_t* sm) :
    state_t(a, sm),
    cf(NULL),
    cstore(a, sm),
    v(a),
    v_set(a),
    uf_idx_store(a, sm),
    var_const_cexpr_store(a, sm),
    cexpr_index_store(a, sm),
    cexpr_deps_store(a),
    unsimplified_constraints(a, sm),
    is_unsat(false),
    unsat_constraints(a),
    seen_function_calls(a),
    processed_asts(a),
    nonnull_cexprs(a),
    not_nonnull_cexprs(a),
    temp_nonnull_cexprs(a),
    tree_idx_store(a, sm),
    loop_cexpr_store(a, sm),
    havoced_cexprs(a),
    cexpr_events_store(a)  {
#if 0
    // CHG: Casting an SM to a store type can't work!!!
    // I don't know what this was really meant to do.
    if (constraint_store_t *cs = dynamic_cast<constraint_store_t *>(sm)) {
        is_unsat = cs->is_unsat;
    } else {
        // This shouldn't be the case
        is_unsat = false;
    }
#endif
}

constraint_store_t::constraint_store_t(
    mc_arena a,
    sm_t* sm,
    const constraint_store_t& other) :
    state_t(a, sm),
    cf(other.cf),
    cstore(a, sm, other.cstore),
    v(other.v, a),
    v_set(other.v_set, a),
    uf_idx_store(a, sm, other.uf_idx_store),
    var_const_cexpr_store(a, sm, other.var_const_cexpr_store),
    cexpr_index_store(a, sm, other.cexpr_index_store),
    cexpr_deps_store(other.cexpr_deps_store, a),
    unsimplified_constraints(a, sm, other.unsimplified_constraints),
    is_unsat(other.is_unsat),
    unsat_constraints(other.unsat_constraints, a),
    seen_function_calls(other.seen_function_calls, a),
    processed_asts(other.processed_asts, a),
    nonnull_cexprs(other.nonnull_cexprs, a),
    not_nonnull_cexprs(other.not_nonnull_cexprs, a),
    temp_nonnull_cexprs(other.temp_nonnull_cexprs, a),
    tree_idx_store(a, sm, other.tree_idx_store),
    loop_cexpr_store(a, sm, other.loop_cexpr_store),
    havoced_cexprs(other.havoced_cexprs, a),
    cexpr_events_store(other.cexpr_events_store, a)  {
    foreach(i, cexpr_deps_store) {
        i->second = clone_cexpr_idx_set(i->second, a);
    }
    foreach(i, cexpr_events_store) {
        i->second = clone_sub_cexpr_event_map(i->second, a);
    }
}

cache_t* constraint_store_t::create_empty_cache(mc_arena a) const
{
    return new (a) constraint_cache_t(a);
}

void constraint_store_t::clear()
{
    v.clear();
    v_set.clear();
    cstore.clear();
    uf_idx_store.clear();
    var_const_cexpr_store.clear();
    cexpr_index_store.clear();
    cexpr_deps_store.clear();
    unsimplified_constraints.clear();
    is_unsat = false;
    unsat_constraints.clear();
    seen_function_calls.clear();
    processed_asts.clear();
    nonnull_cexprs.clear();
    temp_nonnull_cexprs.clear();
    not_nonnull_cexprs.clear();
    tree_idx_store.clear();
    loop_cexpr_store.clear();
    havoced_cexprs.clear();
    cexpr_events_store.clear();
}

void constraint_store_t::insert_var_cexpr_entry(c_expr *var_or_uf, c_expr *cexpr)
{
    if (!var_or_uf || !cexpr) {
        return;
    }
    if (dynamic_cast<c_var *>(var_or_uf) ||
        dynamic_cast<c_uninterpreted *>(var_or_uf)) {
        if (var_or_uf->get_nbits() == cexpr->get_nbits()) {
            var_const_cexpr_store.insert(var_or_uf, cexpr);
        } else {
            if (c_const *cconst = dynamic_cast<c_const *>(cexpr)) {
                var_const_cexpr_store.insert(var_or_uf,
                                             cf->make_cast(cexpr,
                                                           var_or_uf->get_nbits(),
                                                           !cconst->get_unsigned()));
            }
        }
    }
}

// If this c_var or uf corresponds to a constant value, return it, else 0
c_expr *constraint_store_t::get_var_uf_const_value(c_expr *expr)
{
    LET(it, var_const_cexpr_store.find(expr));
    if (it != var_const_cexpr_store.end()) {
        if (dynamic_cast<c_const *>(it->second)) {
            return it->second;
        }
    }
    return 0;
}

cexpr_store_t &constraint_store_t::get_unsimplified_constraints()
{
    return unsimplified_constraints;
}

void constraint_store_t::insert_unsimplified_constraint(
    c_expr *cexpr,
    c_expr *cexpr_unsimpl)
{
    if (!cexpr || !cexpr_unsimpl) {
        return;
    }

    if (!unsimplified_constraints.contains(cexpr)) {
        unsimplified_constraints.insert(cexpr, cexpr_unsimpl);
    }
}

void constraint_store_t::get_constraint_list(list<c_expr *>& constraints)
{
    foreach(it, v) {
        constraints.push_back(*it);
    }
}

void constraint_store_t::get_constraint_list_unsimpl(list<c_expr *>& unsimpls)
{
    foreach(it, v) {
        LET(unsimpl_it, unsimplified_constraints.find(*it));
        if (unsimpl_it == unsimplified_constraints.end()) {
            unsimpls.push_back(*it);
        } else {
            unsimpls.push_back(unsimpl_it->second);
        }
    }
}

bool constraint_store_t::are_constraints_unsat()
{
    return is_unsat;
}

arena_vector_t<c_expr *> &constraint_store_t::get_unsat_constraints()
{
    return unsat_constraints;
}

void constraint_store_t::insert_function_call(const E_funCall *call)
{
    if (call) {
        seen_function_calls.insert(call);
    }
}

bool constraint_store_t::seen_function_call(const E_funCall *call)
{
    return ::contains(seen_function_calls,call);
}

void constraint_store_t::clear_seen_function_calls() {
    seen_function_calls.clear();
}

void constraint_store_t::insert_processed_ast(const ASTNode *astnode) {
    if (astnode) {
        processed_asts.insert(astnode);
    }
}

bool constraint_store_t::processed_ast(const ASTNode *astnode) {
    return ::contains(processed_asts,astnode);
}

void constraint_store_t::clear_processed_asts() {
    processed_asts.clear();
}


bool constraint_store_t::add_constraint(
    c_expr* expr,
    const ASTNode *t,
    c_expr **added_constraint)
{
        ostr_assert(expr->get_nbits() == 1,
                "Adding constraint " << expr << " whose value is not 1-bit?");
    if (expr == cf->Zero(/*nbits*/ 1)) {
        // We are trying to add a constant constraint (before
        // simplification) that is false
        // Simply prune this path. No need to cache, because the problem
        // constraint is exactly this one.
        g_evil_global_cur_abstract_interp->set_kill_path();
    }

        set<c_expr *> substituted_cexprs;
        c_expr *simpl_expr = simplify_cexpr(expr, &substituted_cexprs);
        if (!::contains(v_set,simpl_expr)) {
            v.push_back(simpl_expr);
            v_set.insert(simpl_expr);
            unsimplified_constraints.insert(simpl_expr, expr);
        if (added_constraint) {
            // Return the actual constraint added in added_constraint
            *added_constraint = simpl_expr;
        }

            // Add the dependencies from the new constraint to other
            // constraints that are inlined (for now, constant assignments
            // only) in it. These dependencies can be used to transitively
            // compute all the constraints that affect a particular simplified
            // constraint
            foreach(scit, substituted_cexprs) {
            LET(ci_it, cexpr_index_store.find(*scit));
            ostr_assert(ci_it != cexpr_index_store.end(),
                        "c_expr substituted but assignment not in store?");
            cexpr_idx_set_t *&s =
                cexpr_deps_store[simpl_expr];
            if(!s) {
                s = cexpr_idx_set_t::create(get_arena());
            }
            s->insert(ci_it->second);
            }

            // We simply check the new constraint to see if we can easily
            // detect unsatisfiability. We set the flag is_unsat and do
            // nothing else. constraint-store users can periodically check
            // the flag and take actions like backtracking if unsatisfiable.
            check_new_constraint(simpl_expr, substituted_cexprs);

        if (dynamic_cast<c_unop *>(expr)) {
            // Not (x Ne const) => (x = const)
            c_unop *cunop_simpl = dynamic_cast<c_unop *>(simpl_expr);
            if (cunop_simpl) {
                if (cunop_simpl->get_type() == C_NOT) {
                    c_expr *not_cexpr = cunop_simpl->get_expr();

                    if (not_cexpr->get_type() == C_NE) {
                        c_expr *lhs_expr =
                            static_cast<c_binop *>(not_cexpr)->get_lhs();
                        c_expr *rhs_expr =
                            static_cast<c_binop *>(not_cexpr)->get_rhs();
                        if (dynamic_cast<c_const *>(rhs_expr) &&
                            (!var_const_cexpr_store.contains(lhs_expr))) {
                            insert_var_cexpr_entry(lhs_expr, rhs_expr);
                            // Insert lhs_expr into cexpr_index_store
                            // anyway because that is what will be used
                            // for substitution
                            cexpr_index_store[lhs_expr] = v.size() - 1;
                        }
                        // Map the var or uf being mapped to the index of
                        // the constraint causing this initialization in
                        // the constraint vector v (even for non-constant
                        // rhs's)
                        if (!cexpr_index_store.contains(lhs_expr)) {
                            cexpr_index_store[lhs_expr] = v.size() - 1;
                        }
                    }
                }
            }
            } else if (dynamic_cast<c_binop *>(expr)) {
            c_binop *cbinop_simpl = dynamic_cast<c_binop *>(simpl_expr);
            if (cbinop_simpl) {
                // Constant assignment
                if (cbinop_simpl->get_type() == C_EQ  &&
                    (dynamic_cast<c_var *>(cbinop_simpl->get_lhs()) ||
                     dynamic_cast<c_uninterpreted *>(cbinop_simpl->get_lhs()))) {
                    if (dynamic_cast<c_const *>(cbinop_simpl->get_rhs()) &&
                        // Make sure that we don't already have an
                        // assignment
                        // (This can happen because at the c_expr level, we
                        // can't distinguish between = and ==)
                        (!var_const_cexpr_store.contains(cbinop_simpl->get_lhs()))) {
                        insert_var_cexpr_entry(cbinop_simpl->get_lhs(),
                                               cbinop_simpl->get_rhs());
                        // Insert lhs_expr into cexpr_index_store
                        // anyway because that is what will be used
                        // for substitution
                        cexpr_index_store[cbinop_simpl->get_lhs()] =
                            v.size() - 1;

                    }
                    // Map the var or uf being mapped to the index of
                    // the constraint causing this initialization in
                    // the constraint vector v
                    if (!cexpr_index_store.contains(cbinop_simpl->get_lhs())) {
                        cexpr_index_store[cbinop_simpl->get_lhs()] =
                            v.size() - 1;
                    }
                }
                // Condition expression with same constant assignment
                // (Need this as an optimization for condition expressions
                // to still propagate constants)
                else if (cbinop_simpl->get_type() == C_OR) {
                    c_binop *lhs =
                        dynamic_cast<c_binop *>(cbinop_simpl->get_lhs());
                    c_binop *rhs =
                        dynamic_cast<c_binop *>(cbinop_simpl->get_rhs());
                    if (lhs && rhs && lhs->get_type() == C_AND &&
                        rhs->get_type() == C_AND) {
                        c_unop *rhs_lhs =
                            dynamic_cast<c_unop *>(rhs->get_lhs());
                        c_binop *rhs_rhs =
                            dynamic_cast<c_binop *>(rhs->get_rhs());
                        c_binop *lhs_rhs =
                            dynamic_cast<c_binop *>(lhs->get_rhs());
                        if (rhs_lhs && lhs_rhs && rhs_rhs &&
                            rhs_lhs->get_type() == C_NOT &&
                            lhs_rhs->get_type() == C_EQ &&
                            rhs_rhs->get_type() == C_EQ &&
                            lhs->get_lhs() == rhs_lhs->get_expr() &&
                            lhs_rhs->get_lhs() == rhs_rhs->get_lhs() &&
                            (dynamic_cast<c_var *>(lhs_rhs->get_lhs()) ||
                             dynamic_cast<c_uninterpreted *>(lhs_rhs->get_lhs()))) {
                            if (lhs_rhs->get_rhs() == rhs_rhs->get_rhs() &&
                                dynamic_cast<c_const *>(lhs_rhs->get_rhs()) &&
                                dynamic_cast<c_const *>(rhs_rhs->get_rhs()) &&
                                (!var_const_cexpr_store.contains(lhs_rhs->get_lhs()))) {
                                insert_var_cexpr_entry(lhs_rhs->get_lhs(),
                                                       lhs_rhs->get_rhs());
                                // Insert lhs_expr into cexpr_index_store
                                // anyway because that is what will be used
                                // for substitution
                                cexpr_index_store[lhs_rhs->get_lhs()] =
                                    v.size() - 1;
                            }

                            // Map the var or uf being mapped to the index
                            // of the constraint causing this
                            // initialization in the constraint vector v
                            if (!cexpr_index_store.contains(lhs_rhs->get_lhs())) {
                                cexpr_index_store[lhs_rhs->get_lhs()] =
                                    v.size() - 1;
                            }
                        }
                    }
                }
            }
            }
            return true;
        } else {
            return false;
        }
}

bool constraint_store_t::add_constraint(
    c_expr* expr,
    sub_cexpr_event_map_t& sub_cexpr_events,
    const ASTNode *t,
    c_expr **added_constraint)
{
    c_expr *new_cexpr = 0;
    bool rv = constraint_store_t::add_constraint(expr, 0, &new_cexpr);
    if (rv && new_cexpr) {
        if (added_constraint) {
            *added_constraint = new_cexpr;
        }
        insert_cexpr_events(new_cexpr, sub_cexpr_events);
    }
    return rv;
}

void constraint_store_t::add_loop_assign_constraint(
    c_expr* cexpr,
    c_expr *cexpr_precise,
    const ASTNode *astnode,
    c_expr **added_constraint)
{
    // Check to see if this expression already corresponds to a
    // loop assign constraint
    // If it does, that is a previous loop iteration. That constraint
    // (previously added) needs to be converted into a widened constraint
    // before we add the current (precise) constraint. This way, only the
    // last iteration updates of a variable in a loop are precise, while
    // all previous iterations result in widening.
    LET(tc_it, tree_idx_store.find(astnode));
    if (tc_it != tree_idx_store.end()) {
        // There is already a constraint associated with this expression
        // Swap it out for the widened constraint before adding the new
        // constraint corresponding to this expression
        int idx = tc_it->second;
        LET(lc_it, loop_cexpr_store.find(v[idx]));

        if (lc_it != loop_cexpr_store.end()) {
            v_set.erase(v[idx]);
            LET(cexpr_event_it, cexpr_events_store.find(v[idx]));
            v[idx] = lc_it->second;
            v_set.insert(lc_it->second);
            if (cexpr_event_it != cexpr_events_store.end()) {
                // Assoc the events with the imprecise constraint
                insert_cexpr_events(lc_it->second, *cexpr_event_it->second);
                // Remove the precise constraint
                cexpr_events_store.erase(cexpr_event_it);
            }
            // Erase the constraint from the loop_cexpr_store
            loop_cexpr_store.erase(lc_it);
        }

    }
    tree_idx_store.insert(astnode, v.size());
    loop_cexpr_store.insert(cexpr_precise, cexpr);

    // Actually add the precise constraint for now. It may be swapped out
    // for the widened constraint  if there is another iteration where the
    // same update occurs
    // We don't want to simplify the added constraint before like
    // add_constraint does, because if we need to swap this constraint out,
    // we don't want its effects to linger in the form of constant
    // propagation (through var_const_cexpr_store and such).
    // While not using that constant propagation, there isn't much use to
    // simplifying the c_expr's anyway.
    v.push_back(cexpr_precise);
    v_set.insert(cexpr_precise);
    if (added_constraint) {
        *added_constraint = cexpr_precise;
    }
    unsimplified_constraints.insert(cexpr_precise, cexpr_precise);
}

void constraint_store_t::add_loop_assign_constraint(
    c_expr* cexpr,
    c_expr *cexpr_precise,
    sub_cexpr_event_map_t& sub_cexpr_events,
    const ASTNode *astnode,
    c_expr **added_constraint)
{
    c_expr *new_cexpr = 0;
    constraint_store_t::add_loop_assign_constraint(cexpr,
                                                   cexpr_precise,
                                                   astnode, &new_cexpr);
    if (new_cexpr) {
        if (added_constraint) {
            *added_constraint = new_cexpr;
        }
        insert_cexpr_events(new_cexpr, sub_cexpr_events);
    }
}

cexpr_store_t& constraint_store_t::get_loop_cexpr_store()
{
    return loop_cexpr_store;
}

void constraint_store_t::write_as_text(StateWriter& ostr) const
{
    //  unsimplified_constraints.write_as_text(ostr);
    foreach(it, unsimplified_constraints) {
        ostr << it->first << " simplified from " << endl << it->second;
    }
}

state_t* constraint_store_t::clone(mc_arena a) const
{
    return new (a) constraint_store_t(a, sm, *this);
}

void constraint_store_t::insert(c_expr* key, c_expr* expr)
{
    if ((dynamic_cast<c_var *>(key) ||
         dynamic_cast<c_uninterpreted *>(key)) &&
        (dynamic_cast<c_var *>(expr) ||
         dynamic_cast<c_uninterpreted *>(expr))) {
        cstore.insert(key, expr);
    }
}

void constraint_store_t::erase(c_expr* key)
{
    cstore.erase(key);
}

constraint_store_t::iterator constraint_store_t::find(c_expr* key)
{
    return cstore.find(key);
}

void constraint_store_t::insert(const char* uf_name, unsigned last_idx)
{
    uf_idx_store.insert(uf_name, last_idx);
}

void constraint_store_t::erase(const char *uf_name)
{
    uf_idx_store.erase(uf_name);
}

void constraint_store_t::add_nonnull_cexpr(c_expr *expr)
{
    // Check that it isn't in the not_nonnull set
    if (!::contains(not_nonnull_cexprs,expr) &&
        expr->get_type() != C_CONST) {
        nonnull_cexprs.insert(expr);
    }
}

void constraint_store_t::add_temp_nonnull_cexpr(c_expr *expr)
{
    // Check that it isn't in the not_nonnull set
    if (!::contains(not_nonnull_cexprs,expr) &&
        expr->get_type() != C_CONST) {
        temp_nonnull_cexprs.insert(expr);
    }
}

void constraint_store_t::make_nonnull_cexprs_permanent()
{
    foreach(it, temp_nonnull_cexprs) {
        nonnull_cexprs.insert(*it);
    }
    temp_nonnull_cexprs.clear();
}

bool constraint_store_t::is_nonnull(c_expr *expr)
{
    return ::contains(nonnull_cexprs,expr);
}

void constraint_store_t::add_not_nonnull_cexpr(c_expr *expr)
{
    // Check that it isn't in the nonnull set
    if (!::contains(nonnull_cexprs,expr) &&
        !::contains(temp_nonnull_cexprs,expr) &&
        expr->get_type() != C_CONST) {
        not_nonnull_cexprs.insert(expr);
    }
}

bool constraint_store_t::is_not_nonnull(c_expr *expr)
{
    return ::contains(not_nonnull_cexprs,expr);
}

c_expr *constraint_store_t::get_nonnull_constraints()
{
    // AND all the non-null constraints
    c_expr *rv = cf->One(/*nbits*/ 1);
    foreach(it, nonnull_cexprs) {
        rv = cf->And(rv, cf->Ne(*it, cf->Zero((*it)->get_nbits())));
    }
    return rv;
}

void constraint_store_t::insert_havoced_cexpr(c_expr *cexpr)
{
    havoced_cexprs.insert(cexpr);
}

bool constraint_store_t::is_havoced_cexpr(c_expr *cexpr)
{
    return ::contains(havoced_cexprs,cexpr);
}

const constraint_events_map_t &constraint_store_t::get_constraint_events() const
{
    return cexpr_events_store;
}


// Simplify unsimpl using var_const_cexpr_store
// Return the set of substituted cexpr's in substituted_cexprs (if non-null)
c_expr *constraint_store_t::simplify_cexpr(c_expr *unsimpl,
                                           set<c_expr *> *substituted_cexprs) {
    vector<c_expr*> top_sorted_cexprs;
    top_sort_c_exprs(unsimpl, top_sorted_cexprs);

    // Map that keeps the mapping of the cexpr's already processed from unsimpl
    // to the simplified c_expr's
    c_expr_c_expr_map_t unsimpl_simpl_map;
    foreach(cexprIt, top_sorted_cexprs) {
        c_expr *cur_cexpr = *cexprIt;
        if (c_const *cconst = dynamic_cast<c_const *>(cur_cexpr)) {
            unsimpl_simpl_map[cconst] = cconst;
        } else if (c_var *cvar = dynamic_cast<c_var *>(cur_cexpr)) {
            LET(vc_it, var_const_cexpr_store.find(cvar));
            if (vc_it != var_const_cexpr_store.end()) {
                unsimpl_simpl_map[cvar] = vc_it->second;
                if (substituted_cexprs) {
                    substituted_cexprs->insert(cvar);
                }
            } else {
                unsimpl_simpl_map[cvar] = cvar;
            }
        } else if (c_unop *cunop = dynamic_cast<c_unop *>(cur_cexpr)) {
            c_expr_c_expr_map_t::iterator mapIt =
                unsimpl_simpl_map.find(cunop->get_expr());
            // Top-sortedness ensures that we do have a mapping for the operand
            ostr_assert(mapIt != unsimpl_simpl_map.end(),
                        "unsimpl_simpl_map should contain unop operand");
            unsimpl_simpl_map[cunop] = cf->make_unop(cunop->get_type(),
                                                     mapIt->second);
        } else if (c_binop *cbinop = dynamic_cast<c_binop *>(cur_cexpr)) {
            c_expr_c_expr_map_t::iterator mapIt1 =
                unsimpl_simpl_map.find(cbinop->get_lhs());
            // Top-sortedness ensures that we do have a mapping for the operand
            ostr_assert(mapIt1 != unsimpl_simpl_map.end(),
                        "unsimpl_simpl_map should contain binop operand");
            c_expr_c_expr_map_t::iterator mapIt2 =
                unsimpl_simpl_map.find(cbinop->get_rhs());
            // Top-sortedness ensures that we do have a mapping for the operand
            ostr_assert(mapIt2 != unsimpl_simpl_map.end(),
                        "unsimpl_simpl_map should contain binop operand");
            unsimpl_simpl_map[cbinop] = cf->make_binop(cbinop->get_type(),
                                                       mapIt1->second,
                                                       mapIt2->second);
        } else if (c_cast *ccast = dynamic_cast<c_cast *>(cur_cexpr)) {
            c_expr_c_expr_map_t::iterator mapIt =
                unsimpl_simpl_map.find(ccast->get_expr());
            // Top-sortedness ensures that we do have a mapping for the operand
            ostr_assert(mapIt != unsimpl_simpl_map.end(),
                        "unsimpl_simpl_map should contain cast operand");
            unsimpl_simpl_map[ccast] = cf->make_cast(mapIt->second,
                                                     ccast->get_nbits(),
                                                     ccast->get_sign_extended());
        } else if (c_uninterpreted *cunintr =
                   dynamic_cast<c_uninterpreted *>(cur_cexpr)) {
            vector<c_expr*> simpl_args;
            for(unsigned i = 0; i < cunintr->get_num_args(); i++) {
                c_expr_c_expr_map_t::iterator mapIt =
                    unsimpl_simpl_map.find(cunintr->get_arg(i));
                // Top-sortedness ensures that we do have a mapping for the
                // operand
                ostr_assert(mapIt != unsimpl_simpl_map.end(),
                            "unsimpl_simpl_map should contain uninter operand");
                simpl_args.push_back(mapIt->second);
            }
            c_expr *uf_with_simpl_args = cf->make_ufn(cunintr->get_name(),
                                                      cunintr->get_idx(),
                                                      cunintr->get_nbits(),
                                                      simpl_args);
            LET(vc_it, var_const_cexpr_store.find(uf_with_simpl_args));
            if (vc_it != var_const_cexpr_store.end()) {
                unsimpl_simpl_map[cunintr] = vc_it->second;
                if (substituted_cexprs) {
                    substituted_cexprs->insert(uf_with_simpl_args);
                }
            } else {
                unsimpl_simpl_map[cunintr] = uf_with_simpl_args;
            }
        }
    }

    c_expr_c_expr_map_t::iterator mapIt = unsimpl_simpl_map.find(unsimpl);
    // Top-sortedness ensures that we do have a mapping for the operand
    ostr_assert(mapIt != unsimpl_simpl_map.end(),
                "unsimpl_simpl_map should contain the top expression");
    return mapIt->second;
}

// out_cexpr_set contains all the constraints in in_cexpr_set and the
// constraints that were transtively used to create the simplified constraints
// in in_cexpr_set
// \p out_cexpr_list is in the same order as the constraints in the original
// store
// NB: in_cexpr_set and out_cexpr_set could refer to the same set
void constraint_store_t::get_transitive_deps(
    const VectorBase<c_expr *>& in_cexpr_list,
    vector<c_expr *>& out_cexpr_list) const
{
    set<c_expr *> transitive_deps;
    deque<c_expr *> worklist;
    set<c_expr *> seen_cexprs;
    foreach(ic_it, in_cexpr_list) {
        worklist.push_back(*ic_it);
    }
    while (!worklist.empty()) {
        c_expr *cur_cexpr = worklist.front();
        worklist.pop_front();
        if (!::contains(seen_cexprs,cur_cexpr)) {
            seen_cexprs.insert(cur_cexpr);
            transitive_deps.insert(cur_cexpr);

            LET(cd_it, cexpr_deps_store.find(cur_cexpr));
            if (cd_it == cexpr_deps_store.end()) {
                continue;
            }

            // Iterate over the constraints that cur_cexpr directly depends on
            foreach(cds_it, *cd_it->second) {
                if (::contains(cexpr_deps_store,v[*cds_it])) {
                    // If the constraint has more transitive dependencies
                    worklist.push_front(v[*cds_it]);
                } else {
                    // If the constraint has no more transitive dependencies
                    transitive_deps.insert(v[*cds_it]);
                }
            }
        }
    }
    out_cexpr_list.clear();
    for(unsigned i = 0; i < v.size(); i++) {
        if (::contains(transitive_deps,v[i]) ) {
            out_cexpr_list.push_back(v[i]);
        }
    }
}

// Starting from a set of substituted c_expr's gotten from the simplification
// of some constraint, we get the transitive list of constraintts (in the same
// order as they are in the store) that influenced the substitued c_expr's
void constraint_store_t::get_transitive_deps(
    const set<c_expr *>& substituted_cexprs,
    vector<c_expr *>&out_cexpr_list) const
{
    vector<c_expr *> in_cexpr_list;
    foreach(sc_it, substituted_cexprs) {
        LET(ci_it, cexpr_index_store.find(*sc_it));
        if (ci_it != cexpr_index_store.end()) {
            // Should always be true if we correctly passed in
            // substituted_cexprs from simplify_cexpr
            in_cexpr_list.push_back(v[ci_it->second]);
        }
    }
    get_transitive_deps(in_cexpr_list, out_cexpr_list);
}

void constraint_store_t::check_new_constraint(c_expr *new_simpl_expr,
                                              set<c_expr *>& substituted_cexprs) {
    if (is_unsat) {
        // If the constraints are already unsatisfiable, don't bother
        return;
    }
    // This is basically a sequence of heuristics
    // skowshik: To start off, I am trying to emulate the traditional fpp's
    //
    vector<c_expr *> unsat_cexprs;
    if (new_simpl_expr == cf->Zero(/*nbits*/ 1)) {
        is_unsat = true;
        unsat_cexprs.push_back(new_simpl_expr);
    }
    // Check if Not(new_simpl_expr) is in the store
    else if (::contains(v_set,cf->Not(new_simpl_expr)) ) {
        is_unsat = true;
        unsat_cexprs.push_back(cf->Not(new_simpl_expr));
        unsat_cexprs.push_back(new_simpl_expr);
    }
    // Check Not
    else if (c_unop *cunop = dynamic_cast<c_unop *>(new_simpl_expr)) {
        switch(cunop->get_type()) {
        case C_NOT:
            {
                // Check if the expression that is being NOT'ed is alredy
                // in the store
                if (::contains(v_set,cunop->get_expr())) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cunop->get_expr());
                    unsat_cexprs.push_back(cunop);
                }

                // Check if this expression is Not (x Ne const)
                if (!is_unsat && cunop->get_expr()->get_type() == C_NE) {
                    c_binop *cbinop =
                        static_cast<c_binop *>(cunop->get_expr());
                    if (c_const *const_rhs =
                        dynamic_cast<c_const *>(cbinop->get_rhs())) {
                        LET(vc_it, var_const_cexpr_store.find(cbinop->get_lhs()));
                        if (vc_it != var_const_cexpr_store.end()) {
                            c_const *existing_const =
                                dynamic_cast<c_const *>(vc_it->second);
                            if (existing_const &&
                                existing_const->get_unsigned_long_value() !=
                                const_rhs->get_unsigned_long_value()) {
                                LET(ci_it,
                                    cexpr_index_store.find(cbinop->get_lhs()));
                                if (ci_it != cexpr_index_store.end()) {
                                    is_unsat = true;
                                    unsat_cexprs.push_back(v[ci_it->second]);
                                    unsat_cexprs.push_back(cunop);
                                }
                            }
                        }
                    }
                }
            }
            break;
        default:
            {
                // Do nothing;
            }
        }
    }
    //  Check equal, greater than and less than
    else if (c_binop *cbinop = dynamic_cast<c_binop *>(new_simpl_expr)) {
        switch(cbinop->get_type()) {
        case C_EQ:
            {
                if (v_set.find(cf->Ne(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Ne(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Gt(cbinop->get_lhs(),
                                           cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Gt(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Lt(cbinop->get_lhs(),
                                           cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Lt(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Gtu(cbinop->get_lhs(),
                                            cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Gtu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));

                }
                else if (v_set.find(cf->Ltu(cbinop->get_lhs(),
                                            cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Ltu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                }

                if (is_unsat) {
                    unsat_cexprs.push_back(cbinop);
                }

                // Also look at the constants in the var_const_cexpr_store
                if (!is_unsat) {
                    if (c_const *const_rhs =
                        dynamic_cast<c_const *>(cbinop->get_rhs())) {
                        LET(vc_it, var_const_cexpr_store.find(cbinop->get_lhs()));
                        if (vc_it != var_const_cexpr_store.end()) {
                            c_const *existing_const =
                                dynamic_cast<c_const *>(vc_it->second);
                            if (existing_const &&
                                existing_const->get_unsigned_long_value() !=
                                const_rhs->get_unsigned_long_value()) {
                                LET(ci_it,
                                    cexpr_index_store.find(cbinop->get_lhs()));
                                if (ci_it != cexpr_index_store.end()) {
                                    is_unsat = true;
                                    unsat_cexprs.push_back(v[ci_it->second]);
                                    unsat_cexprs.push_back(cunop);
                                }
                            }
                        }
                    }
                }
            }
            break;
        case C_NE:
            {
                if (v_set.find(cf->Eq(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Eq(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                    unsat_cexprs.push_back(cbinop);
                }
                // Also look at the constants in the var_const_cexpr_store
                if (!is_unsat) {
                    if (c_const *const_rhs =
                        dynamic_cast<c_const *>(cbinop->get_rhs())) {
                        LET(vc_it, var_const_cexpr_store.find(cbinop->get_lhs()));
                        if (vc_it != var_const_cexpr_store.end()) {
                            c_const *existing_const =
                                dynamic_cast<c_const *>(vc_it->second);
                            if (existing_const &&
                                existing_const->get_unsigned_long_value() ==
                                const_rhs->get_unsigned_long_value()) {
                                LET(ci_it,
                                    cexpr_index_store.find(cbinop->get_lhs()));
                                if (ci_it != cexpr_index_store.end()) {
                                    is_unsat = true;
                                    unsat_cexprs.push_back(v[ci_it->second]);
                                    unsat_cexprs.push_back(cunop);
                                }
                            }
                        }
                    }
                }
            }
            break;
        case C_LT:
            {
                if (v_set.find(cf->Eq(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Eq(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Gt(cbinop->get_lhs(),
                                           cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Gt(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Ge(cbinop->get_lhs(),
                                           cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Ge(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }

                if (is_unsat) {
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_LE:
            {
                if (v_set.find(cf->Gt(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Gt(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_LTU:
            {
                if (v_set.find(cf->Eq(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Eq(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Gtu(cbinop->get_lhs(),
                                            cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Gtu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Geu(cbinop->get_lhs(),
                                            cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Geu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                }

                if (is_unsat) {
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_LEU:
            {
                if (v_set.find(cf->Gtu(cbinop->get_lhs(),
                                       cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Gtu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_GT:
            {
                if (v_set.find(cf->Eq(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Eq(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Lt(cbinop->get_lhs(),
                                           cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Lt(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Le(cbinop->get_lhs(),
                                           cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Le(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }

                if (is_unsat) {
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_GE:
            {
                if (v_set.find(cf->Lt(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Lt(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_GTU:
            {
                if (v_set.find(cf->Eq(cbinop->get_lhs(),
                                      cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Eq(cbinop->get_lhs(),
                                                  cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Ltu(cbinop->get_lhs(),
                                            cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Ltu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                }
                else if (v_set.find(cf->Leu(cbinop->get_lhs(),
                                            cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Leu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                }

                if (is_unsat) {
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        case C_GEU:
            {
                if (v_set.find(cf->Ltu(cbinop->get_lhs(),
                                       cbinop->get_rhs())) != v_set.end()) {
                    is_unsat = true;
                    unsat_cexprs.push_back(cf->Ltu(cbinop->get_lhs(),
                                                   cbinop->get_rhs()));
                    unsat_cexprs.push_back(cbinop);
                }
            }
            break;
        default:
            {
                // Do nothing
            }
        }
    }

    if (is_unsat) {
        get_transitive_deps(unsat_cexprs, unsat_cexprs);
        unsat_constraints.clear();
        foreach(uc_it, unsat_cexprs) {
            unsat_constraints.push_back(*uc_it);
        }
    }
}

void constraint_store_t::insert_cexpr_events(c_expr *cexpr,
                         sub_cexpr_event_map_t &sub_cexpr_events)
{
    cexpr_events_store[cexpr] = &sub_cexpr_events;
}

bool constraint_store_with_trees_t::add_constraint(
    c_expr* expr,
    const ASTNode *t,
    c_expr **added_constraint)
{
    c_expr *new_cexpr = 0;
        bool rv = constraint_store_t::add_constraint(expr, 0,
                                                 &new_cexpr);
        if (rv && new_cexpr) {
        if (added_constraint) {
            *added_constraint = new_cexpr;
        }
        if (t) {
            cexpr_trees.insert(new_cexpr, t);
        }
        }

        return rv;
}

bool constraint_store_with_trees_t::add_constraint(
    c_expr* expr,
    sub_cexpr_event_map_t& sub_cexpr_events,
    const ASTNode *t,
    c_expr **added_constraint)
{
    c_expr *new_cexpr = 0;
    bool rv = constraint_store_t::add_constraint(expr, sub_cexpr_events, 0,
                                                 &new_cexpr);
        if (rv && new_cexpr) {
        if (added_constraint) {
            *added_constraint = new_cexpr;
        }
        if (t) {
            cexpr_trees.insert(new_cexpr, t);
        }
        }

        return rv;
}

void constraint_store_with_trees_t::add_loop_assign_constraint(
    c_expr* cexpr,
    c_expr *cexpr_precise,
    const ASTNode *astnode,
    c_expr **added_constraint)
{
    constraint_store_t::add_loop_assign_constraint(cexpr,
                                                   cexpr_precise,
                                                   astnode,
                                                   added_constraint);
    cexpr_trees.insert(cexpr, astnode);
    cexpr_trees.insert(cexpr_precise, astnode);
}

void constraint_store_with_trees_t::add_loop_assign_constraint(
    c_expr* cexpr,
    c_expr *cexpr_precise,
    sub_cexpr_event_map_t& sub_cexpr_events,
    const ASTNode *astnode,
    c_expr **added_constraint)
{
    constraint_store_t::add_loop_assign_constraint(cexpr, cexpr_precise,
                                                   sub_cexpr_events,
                                                   astnode,
                                                   added_constraint);
    cexpr_trees.insert(cexpr, astnode);
    cexpr_trees.insert(cexpr_precise, astnode);
}
