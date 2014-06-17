// state.h
//
// Generic interface to the state for an extension.
//
// Written By: Seth Hallem
// Date: May 21, 2003
//
// (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.
//

#ifndef __STATET_H__
#define __STATET_H__

#include <stdlib.h>
#include <iostream>

#include "arena/arena.hpp"
#include "sm/sm.hpp"
#include "caching/subset-caching.hpp"
#include "libs/asthelp/asthelp-aux.hpp"  // DECL_AST_DOWNCASTS_NONTERMINAL
#include "libs/scoped-ptr.hpp" // specialize scoped_ptr
#include "libs/text/state-writer.hpp" // StateWriter
#include "macros.hpp" // NO_OBJECT_COPIES

// FORWARD REFS
class cache_t;

// class state_t
//
// Generic interface to a 'state', which is whatever the extension defines
// as the state for the purposes of the fixed point calculation.
//
class state_t: public AllocateOnlyOnArena
{
    NO_OBJECT_COPIES(state_t);

public:

    // Construct a state_t with an arena.
    state_t(mc_arena a, sm_t *sm);

    // get the arena for this state_t; this should not be used
    // to free the arena, use 'free_arena' instead
    arena_t *get_arena() const;

    arena_t &get_alloc() { return *get_arena(); }

    // Free the arena associated with this state.
    //
    // NOTE: This frees the 'state_t' object as well!  It must
    // not be accessed again after this call returns.
    void free_arena();

    // Virtual destructor to appease the compiler.
    // Public to allow embedding state_t objects (e.g. in syn_store_t)
    virtual ~state_t() { }

    // Create an appropriate cache for this state.
    virtual cache_t *create_empty_cache(mc_arena a) const = 0;

    // Make a deep copy of this state on the given arena.  The new
    // state_t takes ownership of arena 'a'.
    // NOOTE: Every subclass _MUST_ override this function.
    virtual state_t* clone(mc_arena a) const = 0;

    // Print some debugging information for this state to the stream 'out'.
    // Allows the traversal to print debugging messages that indicate the
    // progress of the analysis.
    // It's expected that this will end with a newline.
    virtual void write_as_text(StateWriter & out) const = 0;

    // Call back at end-of-path.
    // This is used mostly to automatically handle committing models in
    // "source_deriver_state_t"
    // This is called right after sm_t::on_fn_eop
    // By default, it does nothing.
    virtual void on_fn_eop(abstract_interp_t &cur_traversal);

    // Print out the state to 'cout'.  This function is easy to call from a
    // debugger like gdb because it doesn't require the stream argument.
    void debug_print() const;

    // Indicates whether this is a "subset_state_t".
    DECL_AST_DOWNCASTS_NONTERMINAL(subset_state_t);

    // Indicates whether this is an "empty_state_t".
    // Don't provide a downcast, as empty_state_t contains no information.
    virtual bool is_empty_state() const;

    sm_t *sm; // public to ease access
 
    // Removed:
    //virtual size_t compute_hash() const = 0;
    // Was used at one point to get a (rough) count of the number of
    // different states coming from cloning a single state (to be compared
    // with the number of clone operations).  To resurrect a state hash
    // function, see the changeset that introduced this comment.
private:
    
    arena_t *arena;
};

/**
 * Specialize scoped_ptr for a state
 * I tried adding "delete arena" to ~state_t + empty operator delete,
 * but it doesn't work as sometimes states are embedded (e.g. in
 * syn_store_t), causing double-free of the arena
 *
 * SGM: I think that is symptomatic of flawed design.  Who owns a
 * state?  Who owns its arena?  I think the concept of a scoped_ptr
 * for states needs to be rethought.
 *
 * Also, the use of specialization here is particularly dangerous,
 * since scoped_ptr for states behaves quite differently from
 * scoped_ptr for other things.
 **/
template <> struct scoped_ptr<state_t> {
    NO_OBJECT_COPIES(scoped_ptr);
public:
    scoped_ptr(state_t *ptr):ptr(ptr){}
    scoped_ptr():ptr(0){}
    operator state_t*() const { return ptr;}
    state_t* operator->() const { return ptr;}
    state_t& operator*() const { return *ptr;}
    state_t* get() const { return ptr;}
    state_t *release() {
        state_t *tmp = ptr;
        ptr = 0;
        return tmp;
    }
    void reset(state_t *new_ptr = 0) {
        if(ptr)
            ptr->free_arena();
        ptr = new_ptr;
    }
    void swap(scoped_ptr &other) {
        state_t* tmp = ptr;
        ptr = other.ptr;
        other.ptr = tmp;
    }
    ~scoped_ptr() {
        reset();
    }
private:
    state_t *ptr;
};

typedef scoped_ptr<state_t> scoped_state_t;

/**
 * A state for "subset caching".
 * Mainly, it's a set that can be turned into a mapping from
 * expressions to size_t values (subset_hash_t). "unique.hpp" can be
 * used to do that.
 * The advantages of turning the abstract value into a size_t are:
 * - unifies processing (no need to use templates)
 * - potentially saves space for large abstract value (e.g. intervals)
 **/
class subset_state_t : public state_t
{
public:
    subset_state_t(mc_arena a, sm_t *sm);

    /**
     * Turns the state into a "hash", for use in subset caching.
     * In most cases this reflects state quite precisely.
     **/
    virtual void compute_hash(subset_hash_t& v) const = 0;

    /**
     * Used by "false negatives checker"
     * Return a string representation of the value corresponding to
     * the given ID
     **/
    virtual string value_at_id_string(size_t id) const = 0;

    // Does "widen" do anything at all?
    virtual bool widens() const = 0;

    // Widen with all other elements in the same cache.
    virtual void widen(const subset_cache_pair_set_t &cache) = 0;
    // Version that takes a vector, for equality caching. Turns it
    // into a set and calls the above.
    void widen(const VectorBase<pair<const Expression *, size_t> > &cache);
    /**
     * Prints out a "readable" version of \p s by doing a reverse lookup, if possible
     **/
    virtual void print_cache(IndentWriter &out,
                             const subset_cache_pair_set_t &s) const = 0;
    void print_cache(IndentWriter &out,
                     const VectorBase<pair<const Expression *, size_t> > &cache) const;

    struct hash_cache_pair {
        size_t operator()(const pair<const Expression *, size_t> &p)const {
            return (size_t)p.first ^ p.second;
        }
    };

    bool issubset_state_t() const;
};

// class eq_state_t
//
// An interface for states that use the equality cache.
//
class eq_state_t : public state_t {
public:
    // Constructor
    eq_state_t(mc_arena a, sm_t *sm);

    // compare states for equality
    virtual bool is_equal(const eq_state_t& other) const = 0;

    // Widen this state with the state 'other'.  This widening should move the
    // state up the lattice.  By default the widening does nothing.
    // Only called after a back edge
    virtual void widen(const eq_state_t* other);

    cache_t *create_empty_cache( mc_arena a ) const;

    // In the simple case, this vector has size 1.
    virtual size_t compute_hash() const = 0;
};

std::ostream& operator<<(ostream &out, const state_t &s);

#endif // ifndef __STATET_H__
