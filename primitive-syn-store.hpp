#ifndef PRIMITIVE_SYN_STORE_HPP
#define PRIMITIVE_SYN_STORE_HPP

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

    // Remove dangling references in equiv_map to ensure that every
    // equivalence class in the range of 'equiv_map' is in the domain
    // of 'valueof'.  Also ensure that the reference counts are right.

    // 'dangling' also means 'reference is not correct', thus better to clear and
    // recalculate and remove the expr not mapped in valueof.
    void normalize(vector<const Expression *> *removedExprs) {
        
    }

    // Remove all mappings to the equivalence class from equiv_class,
    // and remove the equivalence class from valueof.
    void remove_equiv_class(
        int eq_class,
        vector<const Expression *> *removedExprs = NULL
    ) {
        if(!contains(valueof, eq_class)) return;
        foreach(it, equiv_class) {
            if(it->second == eq_class) {
                removedExprs.add(it->first);
                if(!(--valueof[eq_class].refCount)) {
                    valueof.erase(eq_class);
                    break;
                }
            }
        }
        foreach(it, removedExprs) {
            equiv_class.erase(*it);
        }
    }

    // Set the state for the entire equivalence class
    void insert(const Expression *t, const T& state) {
        int ec;
        insert_and_get_equiv_class(t, state, ec);
    }

    void insert_and_get_equiv_class(
        const Expression *t,
        const T& state,
        int &ec) {
        if(!contains(equiv_class, t)) {
            ec = valueof.first_unmapped_key();
            equiv_class[t] = ec;
            valueof[ec] = EquivClass(state);
        } else {
            valueof[equiv_class[t]].state = state;
        }
    }

    // Erase the entire equivalence class
    bool erase(const Expression *t,
               vector<const Expression *> *removedExprs ) {
        if(!contains(equiv_class, t)) return true;
        remove_equiv_class(equiv_class[t]);
    }

    bool def_ref_count(int ec)
    {
        if(contains(valueof, ec)) {
            if(!(--valueof[ec].refCount)) {
                valueof.erase(ec);
            }
            return true;
        }
        return false;
    }

    // Returns "true" if it was removed.
    bool reset(const Expression *t) {
        if(contains(equiv_class, t)) {
            dec_ref_count(equiv_class[t]);
            equiv_class.erase(t);
            return true;
        }
        return false;
    }

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
    bool erase_all(const Expression *t, vector<const Expression *> *removedExprs = NULL) {
        // for each gets all the removedExprs
        // foreach one call erase(t)

        // or just erase and on the way delete the ec, then normalize
        // afterwards.
        return true;
    }
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
        vector<const Expression *> *removedExprs = NULL) {
        // erase and on the way dec_ref(ec)
        // NB: reflexive!
        return true;
    }

    // Set anything in the store with t as a subexpression to the
    // value.
    void set_all(const Expression *t, const T& val, arena_t *ar) {
        // iterates and set the valueof[ec].state
    }

    // Set anything in the store with t as a subexpression to the
    // value, and also insert t into the store with the value.
    void set_all_insert(const Expression *t, const T& val, arena_t *ar) {
        // insert
        // set_all
    }

    // Simulate an assignment of T = T2 by copying the value from T2
    // and inserting it for T.  Anything that contains T as a subtree
    // is also erased.
    // Returns whether it caused a change.
    bool assign(
        const Expression *t,
        const Expression *t2,
        vector<const Expression *> *removedExprs
    ) {
        int ec1 = get_equiv(t), ec2 = t2 ? get_equiv(t2) : -1;
        return handle_assign(ec1, ec2, removedExprs);
    }

    // Consider "expr" reassigned, without any knowledge of what the
    // new value will be.
    // Returns whether it caused a change.
    bool assign_unknown(
        const Expression *expr,
        vector<const Expression *> *removedExprs) {
        // for union, we need to reassign the union
        return reset_all(expr, true, removedExprs);
    }

    bool remove_for_reassign(
            const Expression *expr,
            vector<const Expression *> *removedExprs) {
        return assign_unknown(expr, removedExprs);
    }

    // Indicates a variable going out of scope (or being dead)
    // Returns whether it caused a change.
    bool unscope(
        const E_variable *expr,
        vector<const Expression *> *removedExprs) {
        assign_unknown(expr, removedExprs);
    }

    // The guts of an 'assign', made available to avoid repeated
    // 'get_equiv' queries in some cases.  't' is the destination
    // of the assign, 'equiv' must be its equivalence class, and
    // 'equiv2' is the equivalence class of the source.
    // Returns whether it caused a change.
    bool handle_assign(
        const Expression *t,
        int ec,
        int ec2,
        vector<const Expression *> *removedExprs = NULL) {
        // remove_for_reassign(t)
        // insert t to e2 
    }

    // Merge equivalence class eq1 into eq2
    // All trees with equiv class eq1 will have eq2 instead; the value for eq1 is discarded
    void merge_equiv(int eq1, int eq2);

    // if either of them exists, set all of them as that value;
    // if all of them exists, merge(eq1, eq2)
    // otherwise, do nothing
    void merge_equiv(const Expression *t1, const Expression *t2);

    ////////// iterators ////////////////////////////

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

    void clear();

    const equiv_map &get_valueof_map() const;

protected:

    // Mapping from trees to equivalance class (int)
    equiv_class_map equiv_class;

    // Mapping from equivalence class to value
    equiv_map valueof;
};
#endif
