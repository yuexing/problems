// expression_store.h
//
// A new kind of store that is actually a mapping from decomposed
// expressions to values.  The goal is to represent complex
// expressions like p + q in a simple way such that if we re-assign
// "p", the relationship attached to "p + q" is automatically cleared.
// We also need to be able to look up expressions quickly.  Thus, if
// we want to know the state attached to p + q, that should be easy to
// get and fast.  For now we do not handle aliasing.  That would be
// possible to add as an extension to this store type.
//
// (c) 2005-2010,2012 Coverity, Inc. All rights reserved worldwide.

#ifndef __EXPRESSION_STORE_H__
#define __EXPRESSION_STORE_H__

#include <set>

#include "store.hpp"
#include "caching/subset-caching.hpp"
#include "flowgraph/block.hpp"
#include "arena/arena.hpp"

class expression_node_t;
struct expression_node_lt {
    bool operator()(const expression_node_t* e1,
                    const expression_node_t* e2) const;
};

// expression_node_t
//
// An expression node has a tree, a value, and a parent edge.
//
class expression_node_t {
public:
    // Parents in the expression tree.
    typedef arena_set_t<expression_node_t*, expression_node_lt> node_list_t;
    typedef node_list_t::iterator iterator;
    typedef node_list_t::const_iterator const_iterator;

    // You specify up front how many edges you need.
    expression_node_t(tree t,
                      void* value);

    // Copy constructor.
    expression_node_t(const expression_node_t& other);

    // Iterate over the parents.
    iterator begin() { return parents.begin(); }
    iterator end() { return parents.end(); }
    const_iterator begin() const { return parents.begin(); }
    const_iterator end() const { return parents.end(); }

    // Iterate over children.
    iterator child_begin() { return children.begin(); }
    iterator child_end() { return children.end(); }

    const_iterator child_begin() const { return children.begin(); }
    const_iterator child_end() const { return children.end(); }

    // Get the value.
    void* get_value(void) const;
    void set_value(void* value);

    // Get the tree.
    tree get_tree(void) const;

    // Add a parent.
    void add_parent(expression_node_t* parent);

    // Add a child.
    void add_child(expression_node_t* child);

    // Intersect parents.
    void intersect_parents(const expression_node_t* other,
                           node_list_t& intersection) const;

private:
    // Parent edges.
    node_list_t parents;    

    // Child edges.
    node_list_t children;

    // The value is an opaque handle.
    void* value;

    // The tree.
    tree t;
};

// Print out an expression node.
std::ostream& operator<<(std::ostream& out, const expression_node_t& en);

// expression_map_t
//
// Encapsulates the expression mapping.
//
class expression_map_t {
public:
    // The set of expression nodes we are tracking currently ...
    typedef arena_set_t<expression_node_t*, expression_node_lt> expression_set_t;
    typedef expression_set_t::iterator iterator;
    typedef expression_set_t::const_iterator const_iterator;
    typedef expression_set_t::size_type size_type;

    // Constructors.
    expression_map_t(mc_arena a);
    expression_map_t(mc_arena a, const expression_map_t& other);

    // Operations on the expression map.

    // Associate "t" with the supplied value.
    bool insert(tree t, void* value);

    // Erase "t".
    void erase(tree t);

    // Assign t to t2.
    void assign(tree t, tree t2, bool do_alias);

    // Reset "t" to to value.
    void reset(tree t, void* value);

    // Find the value attached to a tree.
    iterator find(tree t) const;
    
    // Return the size.
    size_type size(void) const;

    // Iterators.
    iterator begin() { return expr_set.begin(); }
    iterator end() { return expr_set.end(); }
    const_iterator begin() const { return expr_set.begin(); }
    const_iterator end() const { return expr_set.end(); }

private:
    // Private helpers.
    void insert_structure(tree t,
                          expression_node_t* en);

    // Set of expression nodes.
    expression_set_t expr_set;

    // Arena.
    mc_arena arena;
};

//
// An expression store maps expressions to states.  Its purpose is to
// allow an easy way for extensions to manipulate states attached to
// expressions, not just simple trees.  For example, if we attach a
// state to p->q, the world gets complicated if p gets reassigned.  We
// want to simplify that to be a very quick, error free operation.
// The user supplied operations for manipulating the states.
//
// Other template arguments:
// H = hash function (like STL)
// E = equality comparator (defaults to equal_to<T>, which uses operator==)
// W = Widening class with operator() that takes a two stores and
//     modifies the first store to be higher in the lattice.
// A = allocator for T
//
template<class T,
         class MakeValueUnique,
         class E = equal_to<T>,
         class W = null_widen<T>,
         class Cache = subset_cache_t>
class expression_store_t : public subset_state_t {
public:
    // Iterator.
    typedef typename expression_map_t::iterator iterator;
    typedef typename expression_map_t::const_iterator const_iterator;
    typedef typename expression_map_t::size_type size_type;

    typedef expression_store_t<T, MakeValueUnique, E, W, Cache> this_store_t;

    // This should only be called once per function.
    expression_store_t(mc_arena a, sm_t *sm) :
        subset_state_t(a, sm),
        pImpl(new (a) expression_map_t(a)),
        uniq(MakeValueUnique::get_instance()) {
    }

    // Copy a store onto another arena.
    expression_store_t(mc_arena a, sm_t *sm, const this_store_t &o)
        : subset_state_t(a, sm),
          uniq(o.uniq)
    {
        // Copy the expression map.
        this->pImpl = new (a) expression_map_t(a, *(o.pImpl));
    }

    cache_t *create_empty_cache(mc_arena a) const {
        return new (a) Cache;
    }

    // insert's a mapping from the structure on the LHS (t) to the
    // value (state).
    void insert(tree t, const T& state) {
        pImpl->insert(t, (void*)new (this->get_arena()) T(state));
    }

    // Erases "t".  Call this if we lose the value of "t".
    void erase(tree t) {
        pImpl->erase(t);
    }

    // Simulate an assignment of T = T2 by copying the value from T2
    // and inserting it for T.  Anything that contains T as a subtree
    // is also erased.
    void assign(tree t, tree t2) {
        pImpl->assign(t, t2, false);
    }
    
    // Reset is a funny operation.  Given a tree and value, it resets
    // the tree and all parents of that tree that have values to the
    // new value.
    void reset(tree t, const T& state) {
        pImpl->reset(t, (void*)new (this->get_arena()) T(state));
    }

    // Do an alias of T and T2.
    void alias(tree t, tree t2) {
        pImpl->assign(t, t2, true);
    }
    
    iterator find(tree t) {
        iterator it = pImpl->find(t);
        return it;
    }

    void compute_hash(subset_hash_t& v) const {
        foreach(p, *this) {
            if ((*p)->get_value()) {
                // Ignore value-less items - those are structural, not
                // state-carrying.
                tree t = (*p)->get_tree();
                size_t id = uniq->get_unique_id(*((T*)(*p)->get_value()));
                v.insert(make_pair(t, id));
            }
        }
    }

    bool is_equal(const state_t& oth) const {
        this_store_t& other = (this_store_t&) oth;
        if(size() != other.size())
        return false;

        const_iterator p;
        for(p = begin(); p != end(); ++p) {
            const T* q = other.find((*p)->get_tree());
            const T* p_val = (const T*)(*p)->get_value();

            // If both don't have values that is okay.
            if(!q && !p_val)
                continue;

            // If one has no value but the other does, that is not okay.
            if (!q || !p_val)
                return false;
            
            // If they both have values, see what those values are.
            if(!value_equals(*p_val, *q))
                return false;
        }
        return true;
    }

    state_t* clone(mc_arena a) const {
        return new (a) this_store_t(a, sm, *this);
    }

    // Iterators.
    iterator begin() { return pImpl->begin(); }
    iterator end() { return pImpl->end(); }

    const_iterator begin() const { return pImpl->begin(); }
    const_iterator end() const { return pImpl->end(); }

    size_type size() const { return pImpl->size(); }

    const T* operator[](tree t) {
        iterator it = pImpl->find(t);
        if (it == end())
        return NULL;

        return (const T*)(*it)->get_value();
    }

    /**
     * Used if the cache is subset_cache_t.
     **/
    void widen(subset_hash_t::const_iterator begin,
               subset_hash_t::const_iterator end) {
                   call_widen_internal(this,
                                       begin,
                                       end,
                                       value_widener);
               }
    
    /**
     * Used if the cache is subset_cache_t and widen is not null_widen
     **/
    void widen_internal(subset_hash_t::const_iterator begin,
                        subset_hash_t::const_iterator end)
    {
        for(LET(p, begin); p != end; ++p) {
            tree t = p->first;
            // Widen values that are in the store.
            iterator it = find(t);
            if(it != this->end()) {
                if ((*it)->get_value()) {
                    value_widener(this,
                                 (*it)->get_tree(),
                                 (*(T*)(*it)->get_value()),
                                 uniq->get_from_id(p->second));
                }
            }
        }
    }

    string value_at_id_string(size_t id) const {
        ostringstream return_ss;
        return_ss << uniq->get_from_id(id);
        return return_ss.str();
    }


    // Debugging information.
    void write_as_text(ostream& out) const {
        out << "STORE (" << sm->get_sm_name() <<  "): " << endl;
        const_iterator p;
        for(p = begin(); p != end(); ++p) {
            out << (*(*p));
            out << " (parents ";
            foreach(it, (*(*p))) {
                out << (*(*it));
            }
            out << ")" << endl;
        }
        out << "END STORE" << endl;
    }

private:

    // not assignable or copy-constructible for now
    expression_store_t& operator=(const expression_store_t &other) { }
    expression_store_t(const expression_store_t &other) { }
    E value_equals;
    W value_widener;

    expression_map_t* pImpl;
    MakeValueUnique * const uniq;
};

// integer_t
//
// Class encapsulation of an integer ... implement here for speed.
//
class integer_t {
public:
    integer_t(int x) :
        value(x) {
    }
    integer_t(const integer_t& other) :
        value(other.value) {
    }

    integer_t operator=(const integer_t& other) {
        this->value = other.value;
        return *this;
    }

    operator int() const {
        return value;
    }

    int operator*() const {   
        return value;
    }

private:
    int value;
};

bool operator==(const integer_t& i1, const integer_t& i2);

struct integer_hash {
    size_t operator()(const integer_t& i1) const {
        return hash<int>()((int)i1);
    }
};

typedef expression_store_t<tree,
                           tree_unique_t> tree_expression_store_t;
typedef expression_store_t<integer_t,
                           cast_unique_t<integer_t> > integer_expression_store_t;

std::ostream& operator<<(std::ostream& out, const tree_expression_store_t& e);
std::ostream& operator<<(std::ostream& out, const integer_expression_store_t& e);
std::ostream& operator<<(std::ostream& out, const integer_t& i1);

#endif // ifndef __EXPRESSION_STORE_H__
