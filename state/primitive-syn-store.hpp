/**
 * primitive-syn-store.hpp
 *
 * An abstracted, primitive version of the "synonym" store, which
 * supports equivalence classes of expressions. (See syn_store_t.)
 * Has a special method, "assign", which mimicks an assignemnt
 * assign(a, b) discards any previous value in "a" and puts "a" in the
 * same equivalence class as b. Any change to "b"'s state will be
 * reflected on "a", unless the equivalence class is broken down
 * (through e.g. reset_insert, or another assignment)
 *
 * \author Andy Chou
 * \date May 29, 2003
 *
 * If you use a "custom" instantiation (see extern templates at the
 * end, and instantiations in store.cpp), you must include
 * primitive-syn-store-impl.hpp.
 *
 * (c) 2004-2014 Coverity, Inc. All rights reserved worldwide.
 **/

#ifndef PRIMITIVE_SYN_STORE_HPP
#define PRIMITIVE_SYN_STORE_HPP

#include "containers/map.hpp"

#include "caching/subset-caching.hpp"
#include "patterns/extend-patterns.hpp"
#include "store.hpp"
#include "store-pattern.hpp"
#include "ast/cc_ast_aux.hpp" // contains_expr_no_fn
#include "ast/cc.ast.hpp"
#include "allocator/vectormap.hpp" // VectorMapA
#include "containers/resizable-bitset.hpp" // resizable_bitset
#include "containers/foreach.hpp" // foreach
#include "libs/optional/optional-nonnegative-int.hpp"

// class primitive_syn_store_t<T>
//
// A generic mapping from trees to states, with some special functions
// to handle one-level aliasing.  Each tree is mapped to an
// equivalence class (int) by 'equiv_class'.  Then, each equivalence
// class is mapped to a state by 'valueof'.
//
// Template arguments are based on those for store_t<T>.
//
template<class T,
         class Copy = ShallowCopy<T> >
class primitive_syn_store_t {
public:
    /// A typedef for the data type, to use e.g. in templates
    typedef T data_type;
    // We reference-count equivalence classes to avoid making an
    // expensive store iteration for each "reset".
    struct EquivClass {
        EquivClass():refCount(1), value() {}
        EquivClass(int r, const T &v):refCount(r), value(v) {
        }
        explicit EquivClass(const T &v):refCount(1), value(v) {
        }
        int refCount;
        T value;
    };
    // a map from equivalence classes to values.
    typedef VectorMapA<EquivClass> equiv_map;
    // A map from expressions to equivalence classes.
    typedef arena_map_t<const Expression *, int, astnode_lt_t>
    equiv_class_map;

    typedef primitive_syn_store_t<T, Copy> this_store_t;
    typedef size_t size_type;
    typedef typename equiv_map::iterator eiterator;
    typedef std::pair<const Expression * const, eiterator> value_type;

    // Iterator for syn_store_t.
    //
    // This effectively iterates over (key, equiv_class, value)
    // triples.  Use the accessors to get at them individually.
    template <class It = typename equiv_class_map::iterator,
              class store_type = this_store_t,
              class eit = eiterator,
              typename Value = T>
    class iter: public std::iterator<std::bidirectional_iterator_tag,
        pair<const Expression * const, eit>,
        ptrdiff_t,
        pair<const Expression * const, eit> *,
        pair<const Expression * const, eit> &>
    {
    public:

        iter();
        iter(It p, store_type *store);

        iter& operator=(const iter& other);

        Expression const *key() const;
        int equiv_class() const;
        // Reference count of the equivalence class.
        int ref_count() const;
        T value() const;
        Value& valueRef() const;

        // Assume that the store is in normalized form.
        iter& operator++();
        iter operator++(int x);
        iter& operator--();
        iter operator--(int x);

        bool operator==(const iter &other) const;

        bool operator!=(const iter &other) const;

        It p;
        store_type *store;
    };
    typedef iter<> iterator;
    typedef iter<equiv_class_map::const_iterator, const this_store_t, typename equiv_map::const_iterator, const T> const_iterator;

    // Iterator to iterate over all the values. Doesn't give access to
    // the expressions.
    template <class eit = eiterator,
              typename Value = T>
    class value_iter:
        public std::iterator<std::forward_iterator_tag,
        Value,
        ptrdiff_t,
        Value &, Value *> {
    public:
        value_iter();
        value_iter(eit i);

        value_iter& operator=(const value_iter& other);

        Value &operator*() const;
        Value *operator->() const;
        Value &valueRef() const;
        T value() const;

        value_iter& operator++();
        value_iter operator++(int);

        int equiv_class() const;

        bool operator==(const value_iter &other) const;

        bool operator!=(const value_iter &other) const;

        eit p;
    };

    typedef value_iter<> value_iterator;
    typedef value_iter<typename equiv_map::const_iterator, const T> const_value_iterator;

    // This should only be called once per function.
    primitive_syn_store_t(arena_t *a, sm_t *sm);

    // Copy an existing syn_store_t onto another arena.
    primitive_syn_store_t(arena_t *a, const this_store_t &o);

    // Remove dangling references in equiv_map to ensure that every
    // equivalence class in the range of 'equiv_map' is in the domain
    // of 'valueof'.  Also ensure that the reference counts are right.
    void normalize(vector<const Expression *> *removedExprs = NULL);

    // Remove all mappings to the equivalence class from equiv_class,
    // and remove the equivalence class from valueof.
    void remove_equiv_class(
        int eq_class,
        vector<const Expression *> *removedExprs = NULL
    );

    // Determine if the store is normalized.  Return false if not, true if so.
    bool check_normalize() const;

    // Set the state for the entire equivalence class
    void insert(const Expression *t, const T& state, bool use_tree = false);

    void insert_and_get_equiv_class(
        const Expression *t,
        const T& state,
        int &ec,
        bool use_tree = false);

    // Erase the entire equivalence class
    bool erase(const Expression *t,
               vector<const Expression *> *removedExprs = NULL);

    // Must manually call normalize() after several raw_erases
    // After calling raw_erase (and before calling normalize()), you
    // need to check has_value() before using an iterator's valueRef()
    void raw_erase(value_iterator i);
    void erase(value_iterator i);

    void reset(iterator i);

    // Returns "true" if it was removed.
    bool reset(const Expression *t);

    // This invalidates the iterator, including the one you pass in,
    // because if you delete an equivalence class, it deletes other
    // mappings that share that equivalence class.
    // If you want to erase multiple values, use erase_if.
    void erase(iterator i);

    size_type size() const;

    bool erase_all_fields(
        const Expression *t,
        bool *pneed_normalize = NULL,
        vector<const Expression *> *removedExprs = NULL
    );

    /**
     * Erase all keys containing \p t.
     * This DOES erase entire equivalence class for t and all trees containing \p t.
     * If you don't want that use \c reset_all.
     *
     * If removedExprs is not NULL, it will receive all the
     * expressions whose mappings were removed.
     *
     * Returns whether anything was actually erased.
     **/
    bool erase_all(const Expression *t, vector<const Expression *> *removedExprs = NULL);

    bool erase_if(
        bool (*pred)(const iterator &, void *arg),
        void *arg,
        bool *pneed_normalize = NULL,
        vector<const Expression *> *removedExprs = NULL);

    bool erase_if_expr(
        bool (*pred)(const Expression *, void *arg),
        void *arg,
        bool *pneed_normalize = NULL,
        vector<const Expression *> *removedExprs = NULL);

    // Helpers for reset_if_expr/erase_if_expr
    struct IfExpr {
        bool (*pred)(const Expression *, void *arg);
        void *arg;
    };
    static bool if_expr(const iterator &i, void *arg);

    bool reset_all_fields(
        const Expression *t,
        vector<const Expression *> *removedExprs = NULL);

    /**
     * "Reset" all keys containing \p t. This doesn't remove their equivalence class.
     *
     * If "reflexive" is false, this will keep the mapping for "t", if
     * any.
     * Mostly the same as assign_unknown(t), except that reflexiveness
     * is optional, and it will not strip union fields.
     * In most cases, you actually want to use assign_unknown or unscope.
     **/
    bool reset_all(
        const Expression *t,
        bool reflexive = true,
        vector<const Expression *> *removedExprs = NULL);

    bool reset_if(
        bool (*pred)(const iterator &, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    /**
     * Same as above, except the predicate takes an Expression as
     * argument.
     **/
    bool reset_if_expr(
        bool (*pred)(const Expression *, void *arg),
        void *arg,
        vector<const Expression *> *removedExprs = NULL);

    // Erase anything matching a pattern.  Destroys entire
    // equivalence classes.
    bool erase_pattern(
        AST_patterns::ExpressionPattern &pat,
        vector<const Expression *> *removedExprs = NULL);

    // Erase anything matching a pattern.  Does not destroy entire
    // equivalence classes.
    bool reset_pattern(
        AST_patterns::ExpressionPattern &pat,
        vector<const Expression *> *removedExprs = NULL);

    // Set anything in the store with t as a subexpression to the
    // value.
    void set_all(const Expression *t, const T& val, arena_t *ar);

    // Set anything in the store with t as a subexpression to the
    // value, and also insert t into the store with the value.
    void set_all_insert(const Expression *t, const T& val, arena_t *ar);

    // Simulate an assignment of T = T2 by copying the value from T2
    // and inserting it for T.  Anything that contains T as a subtree
    // is also erased.
    // Returns whether it caused a change.
    bool assign(
        const Expression *t,
        const Expression *t2,
        vector<const Expression *> *removedExprs = NULL

    );
    // If T2 is in the store, the result will be identical to
    // assign(t, t2) otherwise there will be no effect.
    // You probably want to use assign unless you know what you're
    // doing.
    void assign_if_present(const Expression *t, const Expression *t2);

    // Consider "expr" reassigned, without any knowledge of what the
    // new value will be.
    // Returns whether it caused a change.
    bool assign_unknown(
        const Expression *expr,
        vector<const Expression *> *removedExprs = NULL);

    // Indicates a variable going out of scope (or being dead)
    // Returns whether it caused a change.
    bool unscope(
        const E_variable *expr,
        vector<const Expression *> *removedExprs = NULL);

    // The guts of an 'assign', made available to avoid repeated
    // 'get_equiv' queries in some cases.  't' is the destination
    // of the assign, 'equiv' must be its equivalence class, and
    // 'equiv2' is the equivalence class of the source.
    // Returns whether it caused a change.
    bool handle_assign(
        const Expression *t,
        int equiv,
        int equiv2,
        vector<const Expression *> *removedExprs = NULL);

    /**
     * Merge equivalence class eq1 into eq2
     * All trees with equiv class eq1 will have eq2 instead; the value for eq1 is discarded
     **/
    void merge_equiv(int eq1, int eq2);

    /*
     * Merge equivalence class of t1 into t2's (similar to an assign)
     * If one of them isn't in store it is added to the other's equiv
     * class.
     */
    void merge_equiv(const Expression *t1, const Expression *t2);

    iterator find(const Expression *t);

    const_iterator find(const Expression *t) const;

    // Return the equivalence class for the given expression, if any.
    opt_uint find_equiv_class(const Expression *expr) const;

    bool contains(const Expression *t) const;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    value_iterator value_begin();
    value_iterator value_end();
    const_value_iterator value_begin() const;
    const_value_iterator value_end() const;

    iterator_pair_wrapper_t<value_iterator> values();
    iterator_pair_wrapper_t<const_value_iterator> values() const;

    bool empty() const;

    T& operator[](const Expression *t);

    // Operations on equivalence classes

    // Get expressions(except 'e') of the equivalence class which 'e' belongs to
    void get_equivs (const Expression *e,
            vector<const Expression *> &equivs) const;

    // Get the equivalence class for a tree, -1 if not found
    int get_equiv(const Expression *t) const;

    // Returns true if both t1 and t2 are in the same equivalence class or point to the same expression
    bool is_equiv(const Expression *t1,const Expression *t2) const;

    /**
     * Returns a tree belonging to the given equivalence class.
     * 0 if not found
     **/
    const Expression *get_tree_from_equiv(int equiv) const;

    size_t equiv_class_count(int equiv_class_num) const;

    // Set the equivalence class for a tree.  If the class is -1, then
    // remove the tree from the mapping if it is there.
    void set_equiv(const Expression *t, int e);

    // Reset the equivalence class for "t" to something fresh.  Set the
    // state to be 'state'.
    void reset_insert(const Expression *t,
                      const T& state);

    // Reset the equivalence class for all elements in the store
    // containing "t" to something fresh.  Set them all to different
    // fresh values.
    void reset_all_insert(const Expression *t, const T& state, arena_t *ar);


    // Get all trees in the given equivalence class that match
    // the given pattern.
    template<typename pat_type> void get_equiv_trees(vector<Expression *>& trees,
                         pat_type &pat,
                         int e) const;

    void clear();

    const equiv_map &get_valueof_map() const;

protected:

    // Remove from store all the expressions that should go away if
    // "expr" is reassigned.
    // See "erase_all" for "removedExprs"
    bool remove_for_reassign(
        const Expression *expr,
        vector<const Expression *> *removedExprs);

    T &existing_valueof_ref(int equiv_class);

    const T &existing_valueof_ref(int equiv_class) const;

    const EquivClass &existing_equiv_class_ref(int equiv_class) const;

    // Decrease the reference count of the equivalence class.
    // Returns true if it drops down to 0.
    bool dec_ref_count(int equiv_class);

    // Mapping from trees to equivalance class (int)
    equiv_class_map equiv_class;

    // Mapping from equivalence class to value
    equiv_map valueof;
};

// Callbacks for erase_if/reset_if.
// Implemented in store.cpp

// arg = address of pattern
// Returns pattern.match(expr)
bool pattern_matches_expr(const Expression *, void *pat);
// arg = expression
// Returns contains_expr_no_fn(haystack, needle, <reflexive>)
bool expr_contains_expr_strict(const Expression *haystack, void *needle);
bool expr_contains_expr_reflexive(const Expression *haystack, void *needle);

extern template class primitive_syn_store_t<int>;
extern template class primitive_syn_store_t<const Expression *>;

#endif
