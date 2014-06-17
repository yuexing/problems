// constraint_store.h
//
// This is a store containing constraints as represented in the constraint
// library. The store contains a set of constraints representing program
// information and maps that represent the latest variable and uninterpreted
// function values.
// This store is directly used by constraint-fpp. Other sm's can derive from
// this store if they require to maintain additional maps in the store
// (c) 2007-2013 Coverity, Inc. All rights reserved worldwide.

#ifndef __CONSTRAINT_STORE_H__
#define __CONSTRAINT_STORE_H__

#include "constraint/constraint.hpp"          // c_const, c_expr, c_*
#include "arena/arena-vector.hpp"             // arena_vector
#include "sm/state/vstore.hpp"                // vstore_t
#include "types/extend-types.hpp"                    // type_t
#include "caching/constraint-cache.hpp"       // constraint_cache_t
#include "ast/cc.ast.hpp"                     // Expression
#include "containers/set.hpp"
#include "containers/map.hpp"

#include "libs/containers/set.hpp"

#include "exceptions/assert.hpp" // ostr_assert

#include "traversal/abstract-interp.hpp" // g_evil_global_cur_abstract_interp

typedef vstore_t<const char *, unsigned, ltstr> uf_last_idx_store_t;

typedef map_c_t<c_expr*, const type_t*, c_expr_lt_t> constraint_type_map_t;

typedef vstore_t<c_expr*, c_expr*, c_expr_lt_t> cexpr_store_t;

// A store that maps a c_expr to an index (into the "v" vector of the
// constraint_store_t)

typedef vstore_t<c_expr *, unsigned, c_expr_lt_t> cexpr_index_store_t;

// Set of c_expr, as indices into the "v" vector of the constraint_store_t.
typedef arena_set_t<unsigned> cexpr_idx_set_t;

cexpr_idx_set_t *clone_cexpr_idx_set(
    const cexpr_idx_set_t *s,
    arena_t *ar);

typedef arena_set_t<
    c_expr *,
    c_expr_lt_t>
    cexpr_set_t;

// Store that maps a c_expr to a set of c_expr's allocated on the function
// arena (as index into "v")

typedef arena_map_t<
    c_expr *,
    cexpr_idx_set_t *,
    c_expr_lt_t> cexpr_indexset_store_t;

typedef vstore_t<const ASTNode*, unsigned, astnode_lt_t> tree_idx_store_t;

// Struct that encapsulates the events associated with a particular c_expr
struct constraint_event_t {
    const char *tag; // e.g. overflow, overflow_assign, on global
                     // string arena
    // Full description of the event (without the part about possible
    // values)
    // On the cur_traversal's event manager.
    const char *description;
    
    bool needs_values; // Does this event need the sample values as provided
    // by the SAT solver when constraints are proved to be satisfiable

    const E_model *call_model;

    typedef arena_map_t<const Expression*, c_expr *, astnode_lt_t>
    operand_map_t;
    operand_map_t operands;

    constraint_event_t(
        arena_t *ar,
        const char *t,
        string const &event,
        bool needs_vals = false,
        const E_model *m = 0);

    constraint_event_t(
        const constraint_event_t &other,
        arena_t *ar);

    constraint_event_t *clone(arena_t *ar) const;
};

typedef arena_map_t<
    c_expr*,
    constraint_event_t *,
    c_expr_lt_t>
sub_cexpr_event_map_t;

sub_cexpr_event_map_t *clone_sub_cexpr_event_map(
    const sub_cexpr_event_map_t *s,
    arena_t *ar);

typedef arena_map_t<c_expr*, sub_cexpr_event_map_t *, c_expr_lt_t> constraint_events_map_t;

class constraint_store_t : public state_t {
public:
    // STL-compatible types
    typedef c_expr* key_type;
    typedef pair<c_expr*, c_expr*> value_type;
    typedef cexpr_store_t::iterator iterator;
    typedef cexpr_store_t::const_iterator const_iterator;
    typedef cexpr_store_t::size_type size_type;

    constraint_store_t(mc_arena a, sm_t* sm);

    constraint_store_t(mc_arena a, sm_t* sm, const constraint_store_t& other);

    cache_t* create_empty_cache(mc_arena a) const;

    void set_cfactory(c_factory *cfactory) { cf = cfactory; }

    c_factory *get_cf() { return cf; }

    // Does this store contain a particular constraint
    bool has_constraint(c_expr *cexpr) {
        return ::contains(v_set, cexpr) ;
    }

    bool contains(c_expr *cexpr) {
        return cstore.contains(cexpr) ;
    }

    virtual bool add_constraint(c_expr* expr, const ASTNode *t = 0,
                                c_expr **added_constraint = 0);

    virtual bool add_constraint(c_expr* expr,
                                sub_cexpr_event_map_t& sub_cexpr_events,
                                const ASTNode *t = 0,
                                c_expr **added_constraint = 0);

    virtual void add_loop_assign_constraint(
        c_expr* cexpr,
        c_expr *cexpr_precise,
        const ASTNode *astnode,
        c_expr **added_constraint = 0);

    virtual void add_loop_assign_constraint(
        c_expr* cexpr,
        c_expr *cexpr_precise,
        sub_cexpr_event_map_t& sub_cexpr_events,
        const ASTNode *astnode,
        c_expr **added_constraint = 0);

    cexpr_store_t& get_loop_cexpr_store();

    void write_as_text(StateWriter& ostr) const;

    const arena_vector_t<c_expr*>& get_constraint_vector() { return v; }

    cexpr_store_t& get_cstore() { return cstore; }

    uf_last_idx_store_t& get_uf_idx_store() { return uf_idx_store; }

    state_t* clone(mc_arena a) const;

    void insert(c_expr* key, c_expr* expr);

    void erase(c_expr* key);

    iterator find(c_expr* key);

    void insert(const char* uf_name, unsigned last_idx);

    void erase(const char *uf_name);

    virtual void clear();

    iterator begin() { return cstore.begin(); }
    iterator end() { return cstore.end(); }

    size_type size() const { return cstore.size(); }

    cexpr_store_t& get_var_const_cexpr_store() {
        return var_const_cexpr_store;
    }

    void insert_var_cexpr_entry(c_expr *var_or_uf, c_expr *cexpr);

    // If this c_var or uf corresponds to a constant value, return it, else 0
    c_expr *get_var_uf_const_value(c_expr *expr);

    cexpr_store_t& get_unsimplified_constraints();

    void insert_unsimplified_constraint(c_expr *cexpr,
                                        c_expr *cexpr_unsimpl);

    void get_constraint_list(list<c_expr *>& constraints);

    void get_constraint_list_unsimpl(list<c_expr *>& unsimpls);

    // Check if constraints in v are trivially diagnosable to be unsatisfiable
    // Useful to know this proactively so we can use it to take actions
    bool are_constraints_unsat();

    arena_vector_t<c_expr *> &get_unsat_constraints();

    void insert_function_call(const E_funCall *call);

    bool seen_function_call(const E_funCall *call);

    void clear_seen_function_calls();

    void insert_processed_ast(const ASTNode *astnode);

    bool processed_ast(const ASTNode *astnode);

    void clear_processed_asts();

    void add_nonnull_cexpr(c_expr *expr);

    void add_temp_nonnull_cexpr(c_expr *expr);

    void make_nonnull_cexprs_permanent();

    bool is_nonnull(c_expr *expr);

    void add_not_nonnull_cexpr(c_expr *expr);

    bool is_not_nonnull(c_expr *expr);

    c_expr *get_nonnull_constraints();

    void insert_havoced_cexpr(c_expr *cexpr);

    bool is_havoced_cexpr(c_expr *cexpr);

    const constraint_events_map_t& get_constraint_events() const;

    // Function that simplifies unsimpl by substituting from 
    // var_const_cexpr_store
    // This is a useful constraint-util, but is currently stuck here because it
    // is tied to constraint_store_t
    c_expr *simplify_cexpr(c_expr *unsimpl,
                           set<c_expr *> *substituted_cexprs = 0);

    // Given a list of c_expr's (in_cexpr_list) in the store, compute the list 
    // of constraints
    // from the store that are transitively used to simplify the constraints in
    // in_cexpr_list.
    // All the constraints in in_cexpr_list should be in the store
    void get_transitive_deps(const VectorBase<c_expr*>& in_cexpr_list,
                             vector<c_expr *>& out_cexpr_list) const;
    // Given a set of substituted c_expr's (clearly, these c_expr's should be
    // in the cexpr_idx_store as they are initialized by some constraint in the
    // store), compute the list of constraints from the store that 
    // transitively influence the c_expr's in substituted_cexprs
    void get_transitive_deps(const set<c_expr *>& substituted_cexprs,
                             vector<c_expr *>&out_cexpr_list) const;

private:
    // Heuristically check if new_expr is easily detectable to be unsatisfiable
    // with a constraint that is already in the store. This is used to
    // proactively find unsatisfiabilities without calling the solver in the
    // same way that the old fpp's did it.
    // NB: We assume that new_expr is already simplified
    void check_new_constraint(c_expr *new_simpl_expr,
                              set<c_expr *>& substituted_cexprs);

    // Inserts events corresponding to cexpr
    // This is private because the events need to be associated with the
    // simplified constraints in the store. Clients don't know about
    // simplification
    void insert_cexpr_events(c_expr *cexpr, 
                             sub_cexpr_event_map_t &sub_cexpr_events);


private:
    c_factory *cf;   // The c_factory used to generate the c_expr's in store

    // A mapping from structural c_expr's with a 0-index to those with the
    // correct index.
    cexpr_store_t cstore;

    // The key invariant that we need to ensure is that indices of this
    // vector are maintained by other parts of this system (backtrack stack).
    // It is expected that an index maintains a constraint corresponding
    // to a particular AST constantly (widening the constraint is ok, but not
    // changing the order of various constraints in the vector)
    arena_vector_t<c_expr*> v;
    // Set to check for membership in the vector fast
    cexpr_set_t v_set;

    // A mapping from the name of an uninterpreted function to the last index
    // used to update an instance of the uninterpreted function with *some*
    // arguments
    // NB: The char* used for the names of the uf's are the unique pointers
    // (strings) maintained within the c_factory cf
    uf_last_idx_store_t uf_idx_store;

    // A map tracking the cexpr's corresponding to variables and uf's
    // NB: We ONLY use this to keep track of vars and uf's that are constants
    //     to keep the size of this map small. Also, that is its only use.
    // We use this map to substitute constants for variables and uf's
    // This propagates constants proactively thus invoking the
    // solver only when it is absolutely essential. Another benefit of keeping
    // the c_expr's corresponding to the latest value of a var/uf in a map
    // is that the better information (e.g., is it a constant?) can be used
    // to make better decisions downstream.
    cexpr_store_t var_const_cexpr_store;

    // A mapping from a cexpr (a var or a uf) to the constraint that
    // initialized the cexpr (if we saw an initialization)
    cexpr_index_store_t cexpr_index_store;

    // Map from each simplified constraint in the store to the set of previous
    // constraints (indices) that were required to simplify the current
    // constraint.
    // A transitive closure of these index sets shows all the constraints that
    // were used to arrive at the simplified constraint
    cexpr_indexset_store_t cexpr_deps_store;

    // With our aggressive constant propagation, we need a map from each
    // constraint to the corresponding unsimplified (without constant
    // propagation) constraints. This is useful when we establish a chain of
    // constraints affecting a constraint. (We are missing this in the
    // constraints themselves because the vars and uf's that are constants are
    // substituted)
    cexpr_store_t unsimplified_constraints;

    // Is the set of constraints in v *known* to be unsatisfiable by
    // some trivial algorithms. E.g. two constraints contradicting each other.
    // is_unsat is false otherwise.
    bool is_unsat;
    // If is_unsat is true, then this contains the constraints that caused it
    // to be unsatisfiable
    arena_vector_t<c_expr *> unsat_constraints;

    // Seen function calls along this path. We need this because we want to
    // refresh a function call only if it is actually a new one, not if it
    // is the same function call represented in the model and the call itself
    typedef
    arena_set_t<const E_funCall*, astnode_lt_t> seen_function_call_set;
    seen_function_call_set seen_function_calls;

    // AST's that have been already processed. This is useful for select
    // expressions (e.g. assignments) that we don't want to reprocess, if
    // we already have (otherwise the logic changes).
    // Basically, this should contain all AST's that perform updates (ie.
    // refresh variables or uf's)
    typedef
    arena_set_t<const ASTNode*, astnode_lt_t> processed_ast_set;
    processed_ast_set processed_asts;

    // While inserting into the sets below, take care so that they are
    // consistent. The nonnull_cexprs set contains the ptr expr's that are
    // clearly non-null. The not_nonnull_cexprs set contain the ptr expr's that
    // are clearly null (as evidenced by the analysis seeing them assigned to
    // other expressions or compared to null)
    // Set of c_expr's that are definitely non-null
    cexpr_set_t nonnull_cexprs;
    // Set of c_expr's for which there is definite evidence that it can be null
    cexpr_set_t not_nonnull_cexprs;
    // Set of c_expr's that are non-null, but are waiting to be added at
    // statement boundaries
    cexpr_set_t temp_nonnull_cexprs;

    // This maps widening constraints that are generated from loop assignments
    // to the more precise constraint representing the same assignment
    // This store is required because, while we want to widen stuff inside a
    // loop, we also want the precise constraint in the last iteration of a
    // loop (this is definitely NOT COMPLETE, but it captures the common case)
    // We switch the widened constraint for the precise constraint when we
    // are about to solve all the constraints
    tree_idx_store_t tree_idx_store;
    cexpr_store_t loop_cexpr_store;

    // The set of havoc'ed values
    cexpr_set_t havoced_cexprs;

    // A mapping from a constraint to the set of events inside it
    // Each event is a mapping from a sub-cexpr of the constraint to
    // a constraint_event_t
    constraint_events_map_t cexpr_events_store;
};

typedef vstore_t<c_expr *, const ASTNode*, c_expr_lt_t> c_expr_tree_t;

// Constraint store with an additional map from c_expr to NEW AST trees, which
// is useful for error reporting with checkers
class constraint_store_with_trees_t : public constraint_store_t {
public:
    constraint_store_with_trees_t(mc_arena a, sm_t *sm) :
        constraint_store_t(a, sm), cexpr_trees(a, sm) {
    }

    constraint_store_with_trees_t(mc_arena a, sm_t *sm,
                                  const constraint_store_with_trees_t& other) :
        constraint_store_t(a, sm, other),
        cexpr_trees(a, sm, other.cexpr_trees) {
    }

    state_t* clone(mc_arena a) const {
        return new (a) constraint_store_with_trees_t(a, sm, *this);
    }

    c_expr_tree_t& get_cexpr_trees_map() { return cexpr_trees; }

    virtual bool add_constraint(
        c_expr* expr,
        const ASTNode *t,
        c_expr **added_constraint = 0);

    bool add_constraint(
        c_expr* expr,
        sub_cexpr_event_map_t& sub_cexpr_events,
        const ASTNode *t,
        c_expr **added_constraint = 0);

    void add_loop_assign_constraint(
        c_expr* cexpr,
        c_expr *cexpr_precise,
        const ASTNode *astnode,
        c_expr **added_constraint = 0);

    void add_loop_assign_constraint(
        c_expr* cexpr,
        c_expr *cexpr_precise,
        sub_cexpr_event_map_t& sub_cexpr_events,
        const ASTNode *astnode,
        c_expr **added_constraint = 0);

    virtual void clear() {
        constraint_store_t::clear();
        cexpr_trees.clear();
    }

private:
    c_expr_tree_t cexpr_trees;
};

#endif
