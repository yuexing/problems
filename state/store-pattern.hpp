// (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
/**
 * \file store-pattern.hpp
 * Pattern to match expresstions in a "store"
 **/

#ifndef STORE_PATTERN_HPP
#define STORE_PATTERN_HPP

#include "analysis/ast/patterns/extend-patterns.hpp"                   // ExpressionPattern
#include "analysis/errrep/errrep-fwd.hpp"        // err_t
#include "analysis/sm/model-fwd.hpp"             // model_t
#include "analysis/sm/checker.hpp"               // checker_t

/**
 * A pattern that tests if something is in the store
 * Only matches expressions
 **/
template<typename store_t> class StorePattern: public ExpressionPattern {
public:
    StorePattern(store_t *store):store(store) {}

    void print(ostream &out) const {
        out << "store pattern";
    }
    // Note: this returns the *specific* key that the store uses, which may
    // be different from the expression that this patterns was matched against.
    Expression const *get_key() const {
        return get_iterator().key();
    }
    typename store_t::data_type &get_value() const {
        return get_iterator().valueRef();
    }
    bool has(const Expression *expr,
             typename store_t::data_type val) {
        return match(expr) && get_value() == val;
    }
    int get_equiv_class() const {
        return get_iterator().equiv_class();
    }
    int equiv_class_count() const {
        return store->equiv_class_count(get_equiv_class());
    }
    err_t *find_error() const {
        return store->find_error(last_expr());
    }
    model_t *find_model() const {
        return store->find_model(last_expr());
    }
    ExpressionPattern &operator=(ExpressionPattern &p) {
        return ExpressionPattern::operator=(p);
    }
    ExpressionPattern &operator=(StorePattern &p) {
        return ExpressionPattern::operator=(p);
    }
    // For use in reset / erase
    const typename store_t::iterator &get_iterator() const {
        // For the assert
        (void)last_expr();
        return i;
    }
    void erase_from_store() {
        store->erase(get_iterator());
    }
    void reset_from_store() {
        store->reset(get_iterator());
    }
    err_t *commit_and_erase_error(err_t *error,
                                abstract_interp_t &cur_traversal) {
        error = 
            static_cast<checker_t *>(store->sm)->commit_error(error, cur_traversal);
        erase_from_store();
        return error;
    }
    // Indicates that "e" is in store and has an associated error, in
    // which case the error is stored in "err"
    bool has_error(const Expression *t,
                   err_t *&err) {
        if(!this->match(t))
            return false;
        err_t *e = find_error();
        if(!e)
            return false;
        err = e;
        return true;
    }

    // Set the store to search in.  This isn't the cleanest way to do
    // things, but it's necessary in order allocate this pattern statically
    // (which is also not the cleanest way to do things).
    void set_store(store_t *s) {
        store = s;
    }
protected:
    bool matches(const Expression *t) {
        return (i = store->find(t)) != store->end();
    }
    typename store_t::iterator i;
    store_t *store;
};


/**
 * A pattern that tests if an expression is in the store, and it has a particular
 * abstract value.
 **/
template<typename store_t> class StoreValuePattern: public StorePattern<store_t> {

public:
    StoreValuePattern(store_t *store,
                      const typename store_t::data_type & value) :
        StorePattern<store_t>(store),
        value(value) {}

    void print(ostream &out) const {
        out << "store pattern when value=" << value;
    }

private:
    // not for use

    StoreValuePattern(store_t *store);
    bool has(const Expression *expr,
             typename store_t::data_type val);

protected:
    bool matches(const Expression *t) {
        return StorePattern<store_t>::matches(t) &&
               (value == StorePattern<store_t>::get_value());
    }
    const typename store_t::data_type value;
};

template<typename store_t> StorePattern<store_t> &
ast_store_pattern(store_t *store) {
    return *new(current_pattern_arena) StorePattern<store_t>(store);
}

#define DECL_STORE_PAT(s, store)\
StorePattern<typeof(*store)> s(store)

#define DECL_STORE_VAL_PAT(s, store, value)\
StoreValuePattern<typeof(*store)> s(store, value)

#endif // STORE_PATTERN_HPP
