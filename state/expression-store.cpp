// expression_store.c
//
// Implementation of the expression store.
//
// (c) 2005-2007,2010-2013 Coverity, Inc. All rights reserved worldwide.

#include "expression-store.hpp"

using namespace std;

// expression_map_t::insert
//
// Insert an expression into the expression map.
//
bool expression_map_t::insert(tree t, void* value)
{
    // We don't handle the case of a collision in the store.  The user
    // should either make sure that this doesn't happen or do
    // something appropriate.
    if (contains(*this,t))
        return false;

    // Insert "t" into the store with an associated value.
    expression_node_t* en = new (arena) expression_node_t(t, value);
    expr_set.insert(en);

    // Now recurse over all of the children and insert expression
    // nodes for each child with the appropriate parents.
    this->insert_structure(t, en);

    return true;
}

// expression_map_t::insert_structure
//
// Recurse over children and insert them into the map pointing back to
// the parent appropriately.
//
void expression_map_t::insert_structure(tree t,
                                        expression_node_t* en)
{
    for(LET(it, t->child_begin());
        it != t->child_end();
        ++it) {
        expression_node_t* child = NULL;

        // Dummy for lookup purposes.
        expression_node_t dummy_n(*it, NULL);
        expression_set_t::iterator eset_it = expr_set.find(&dummy_n);
        if (eset_it != expr_set.end()) {
            child = (*eset_it);
        } else {
            child = new (arena) expression_node_t(*it, NULL);
            expr_set.insert(child);
        }

        child->add_parent(en);
        en->add_child(child);
        
        this->insert_structure((*it), child);
    }            
}

// expression_map_t::erase
//
// Erases a tree from the expression map.
//
void expression_map_t::erase(tree t)
{
    expression_node_t en(t, NULL);
    iterator it = expr_set.find(&en);
    if (it != end()) {
        expression_node_t* dead_en = (*it);
        expr_set.erase(dead_en);
        foreach(child_it, (*dead_en)) {
            this->erase((*child_it)->get_tree());
        }
    }
}

// expression_map_t::find
//
// Return a pointer to the value associated with the given tree or
// NULL if no such value exists.
//
expression_map_t::iterator expression_map_t::find(tree t) const
{
    expression_node_t en(t, NULL);
    return expr_set.find(&en);
}

// expression_map_t::assign
//
// Do an assignment.  The last parameter controls whether or not this
// is an alias.
//
void expression_map_t::assign(tree t, tree t2, bool do_alias)
{
    // XXX: Not implemented.
}

// expression_map_t::reset
//
// Do a reset.  Starting at the given tree, traverse the tree and all
// parents recursively.  Reset anything with a value to have the
// supplied value.
//
void expression_map_t::reset(tree t, void* value)
{
    iterator it = find(t);
    if (it != end()) {
        (*it)->set_value(value);

        foreach(parent_it, *(*it)) {
            // Slightly inefficient - could overload reset to take an iterator ...
            this->reset((*parent_it)->get_tree(), value);
        }
    }
}

// expression_map_t::expression_map_t
//
// Basic constructor.
//
expression_map_t::expression_map_t(mc_arena a) :
    arena(a)
{
}

// expression_map_t::expression_map_t
//
// Copy constructor.
//
expression_map_t::expression_map_t(mc_arena a, const expression_map_t& other) :
    arena(a)
{
    // First clone all of the tree->value mappings.
    foreach(it, other) {
        this->expr_set.insert(new (a) expression_node_t(*(*it)));
    }

    // Now fix up all of the parent pointers.
    foreach(it, other) {
        iterator new_expr_it = this->expr_set.find(*it);
        ostr_assert(new_expr_it != this->expr_set.end(), "There is no mapping for an expression that we should have just copied.");

        bool has_parent = false;
        foreach(parent_it, (*(*it))) {
            iterator parent_expr_it = this->expr_set.find(*parent_it);

            // We can have dangling parents because it may be the case
            // that the parent expression was deleted if a sibling
            // sub-expression was re-assigned.  This is an opportunity
            // for compression if (1) it turns out that the expression
            // we are looking at has 0 present parent pointers and (2)
            // this expression has no value.
            if (parent_expr_it != this->expr_set.end()) {
                (*new_expr_it)->add_parent(*parent_expr_it);
                (*parent_expr_it)->add_child(*new_expr_it);
                
                has_parent = true;
            }
        }

        if (!has_parent && !((*new_expr_it)->get_value())) {
            // No parents, no value.  Erase the node.
            this->expr_set.erase(new_expr_it);
        }
    }
}

// expression_map_t::size
//
// Return the size of the expression map.
//
expression_map_t::size_type expression_map_t::size(void) const
{
    return expr_set.size();
}

// expression_node_t::expression_node_t
//
// Expression node constructor.
//
expression_node_t::expression_node_t(tree t,
                                     void* value) :
    value(value),
     t(t)
{
}

// expression_node_t::get_value
//
// Return a value.
//
void* expression_node_t::get_value(void) const
{
    return this->value;
}

// expression_node_t::set_value
//
// Set the value of an expression node.
//
void expression_node_t::set_value(void* value)
{
    this->value = value;
}

// expression_node_t::get_tree
//
// Get a tree.
//
tree expression_node_t::get_tree(void) const
{
    return this->t;
}

// expression_node_t::expression_node_t
//
// Copy constructor for the expression node.
//
expression_node_t::expression_node_t(const expression_node_t& other) :
    value(other.value),
     t(other.t)
{
    // XXX: DO NOT COPY THE PARENT LIST.  Those are generally dangling
    // pointers.  Those should be copied in the wrapper cloning
    // function.
}

// expression_node_t::intersect_parents
//
// Returns the intersection of the parents of 'this" iwth the parents
// of "other" in the out-set "intersection".
//
void expression_node_t::intersect_parents(const expression_node_t* other,
                                          node_list_t& intersection) const
{
    set_intersection(this->begin(), this->end(),
                     other->begin(), other->end(),
                     inserter(intersection, intersection.begin()),
                     expression_node_lt());
}

// expression_node_t::add_parent
//
// Add a parent to this expression node's parent list.
//
void expression_node_t::add_parent(expression_node_t* parent)
{
    this->parents.insert(parent);
}

// expression_node_t::add_child
//
// Add a child to this expression node's child list.
//
void expression_node_t::add_child(expression_node_t* child)
{
    this->children.insert(child);
}

/////////////////////
// integer_t - simple helper for integer expression stores.
/////////////////////
bool operator==(const integer_t& i1, const integer_t& i2)
{
    return ((int)i1 == (int)i2);
}

/////////////////////
// STL helpers.
/////////////////////

// expression_node_lt::operator()
//
// Less-than comparison on expression nodes.  Just use the tree
// because we cannot have >1 value to a single tree.
//
bool expression_node_lt::operator()(const expression_node_t* e1,
                                    const expression_node_t* e2) const
{
    return lttree()(e1->get_tree(), e2->get_tree());
}

// operator<< for expression_node_t
//
// Print out expression nodes for debugging.
//
std::ostream& operator<<(std::ostream& out, const expression_node_t& en)
{
    out << en.get_tree() << " -> " << (en.get_value() ? en.get_value() : "null");
    return out;
}

// operator<< for tree_expression_store_t
//
// Print out expression store for debugging purposes.
//
std::ostream& operator<<(std::ostream& out, const tree_expression_store_t& e)
{
    e.write_as_text(out);
    return out;
}

// operator<< for integer_expression_store_t
//
// Print out expression store for debugging purposes.
//
std::ostream& operator<<(std::ostream& out, const integer_expression_store_t& e)
{
    e.write_as_text(out);
    return out;
}

// operator<< for integer_t
//
// Print out the enclosed integer.
//
std::ostream& operator<<(std::ostream& out, const integer_t& i1)
{
    out << (int)i1;
    return out;
}
    
