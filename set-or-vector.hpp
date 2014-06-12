#ifndef SET_OR_VECTOR_HPP
#define SET_OR_VECTOR_HPP

/**
 * set and map can be reduced from each other:
 * 1) given a map, a set can be implemented with k==v;
 * 2) given a set, a map can be implemented with value_type(k, v)
 * 3) set using red-black tree takes more space, however, vector is not sorted.
 * Thus create a datastruct: insert to set, and periodically spill to vector.
 * The question is: 
 * *) when to spill, and how big are we going to alloc for the vector.
 */

#include <set>
#include <map>
using namespace std;

// implement 1)
template<class T,
    class Compare = less<T>,
    class Alloc = allocator<T>>
struct set_from_map 
{

    typedef map<T, Compare, Alloc> map_type;



};

// implement 2)
struct map_from_set
{

};

// implement 3)
struct set_or_vector
{

};
#endif /* SET_OR_VECTOR_HPP */
