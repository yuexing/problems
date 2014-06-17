// filename-class-impl.hpp
// implementation header for filename-class.hpp
// (c) 2008-2013 Coverity, Inc. All rights reserved worldwide.

// This file describes the data structure that is used behind the
// scenes by Filename.  It is a tree of name components, mirroring (a
// subset of) the structure of the file system that the file names
// refer to.
//
// Each node carries a reference count of the number of Filename
// objects or child FilenameNodes referring to it.  When the reference
// count goes to zero, the node is deallocated.

#ifndef FILENAME_CLASS_IMPL_HPP
#define FILENAME_CLASS_IMPL_HPP

#include "filename-class.hpp"          // external decls for this module

#include "libs/containers/array.hpp"   // ArrayStack

#include <limits.h>                    // USHRT_MAX
#include <vector>                      // vector

OPEN_NAMESPACE(FilenameNS)


// Filename node table
//
// A FilenameNode::Index is an index into this array,
// an alternative and more compact reference than
// storing a pointer.  Looked up by index via
// FilenameNode::get(index).
//
// When a node is destroyed, its index is placed
// onto a free list and re-used.  The size of this
// array should never exceed the peak number of nodes.
//
// This pointer is initially NULL, and is initialized
// on demand when getSuperRootNode is called.  The
// first element will always be the super-root.
// It is never deallocated.
extern std::vector<FilenameNode *> *filenameNodes;


// A single node in the name component tree.
class FilenameNode {

public:      // data
    // Parent node in the tree, or NULL for the super-root.  This
    // reference is counted in the parent's reference count.
    FilenameNode * /*nullable*/ parent;

    // Name at this node.  Heap-allocated, owned by 'this'.  Never
    // NULL, but may be empty.
    //
    // I do not use the string class because I do not want to pay the
    // overhead for a reference count and size/length.
    //
    // For a root name, the first letter encodes the absolute marker,
    // using "/" for absolute and " " for relative.  The first letter
    // is not part of the filesystem name, however.
    char * /*owner*/ name;

    // Child nodes, i.e., those whose names begin with the name
    // encoded by the 'this node.  Maintained in sorted order
    // according to the 'strcmp' order on the contained names, which
    // are unique within a node.
    //
    // Although the lifetime of the parent encloses the lifetime of
    // every child, I do not call these owner pointers because the
    // nodes are most directly deallocated in response to reference
    // count activity.
    ArrayStack<FilenameNode*> children;

    // Index of node in global table.
    FilenameNodeIndex index;

    // Reference count.  When it goes to zero, this object removes
    // itself from its parent's (if any) 'children' array and
    // deallocates itself.
    //
    // If it hits MAX_REFCT, then it stops being tracked, and will
    // never decrease afterward.
    unsigned short refct;
    enum { MAX_REFCT = USHRT_MAX };
    
    // Depth in name tree.  The super-roots has depth 0, user roots
    // have depth 1, etc.  An exception is thrown if an attempt is
    // made to create a name with depth greater than MAX_DEPTH.
    unsigned short depth;
    enum { MAX_DEPTH = USHRT_MAX };
    
private:     // funcs
    // Private to force clients to go through 'decRefct'.  Remove
    // myself from parent list and decrement parent's refct if
    // I have a parent.
    ~FilenameNode();

    // The parent half of the constructor.  Bumps this->refct.
    void insertChild(FilenameNode *child, int nameLen);

    // And similar for the destructor.
    void removeChild(FilenameNode *child);

    // Find the index in 'children' of a child with the given name.
    // Return false if none exists, setting 'index' to the proper
    // location to insert a child with that name.
    bool findChildIndex(int &index /*OUT*/,
                        char const *name, int nameLen);
                                  
public:      // funcs
    // Create an object with reference count 1 and the given name.  If
    // 'parent' is not NULL, adds itself to its parent's 'children'
    // array and bumps the parent's reference count.
    //
    // There must not already be a child of 'parent' with this 'name'.
    FilenameNode(FilenameNode * /*nullable*/ parent,
                 char const *name, int nameLen);

    // Returns the filename node for an index in the
    // global table.
    static FilenameNode * get(FilenameNodeIndex idx) {
        return (*filenameNodes)[idx];
    }

    // manipulate the refct
    void incRefct();
    void decRefct();         // may deallocate 'this'

    bool isRoot() const { return depth == 1; }
    
    // Follow 'parent' pointers until arriving at a root.
    FilenameNode const *getRoot() const;

    // If this node has a child with 'name', return it, otherwise
    // return NULL.  Do not modify any reference count.
    FilenameNode *findChild(char const *name, int nameLen);

    // Get or create a child node with 'name'.  If created, set its
    // refct to 1, otherwise bump it.
    FilenameNode *getOrCreateChild(char const *name, int nameLen);

    // implement part of Filename::compare, looking at another node
    // that is at the same level as 'this'
    // If "commonPrefixSize" is not NULL, it gets the number of common
    // components starting at the root.
    Filename::FileComparison compareToIsoLevel
    (FilenameNode *other,
     bool case_insensitive,
     int *commonPrefixSize);

    // the core of Filename::compare(char*), but needs some help at
    // the end from the caller; see callsite for details
    Filename::FileComparison compareToString
    (char const *&str,
     Filename::FilenameInterp interp,
     bool case_insensitive);

    // write using the same format as Filename is specified to use
    ostream& write(ostream &os, char sep) const;

    // Write with XML escaping.
    void writeToFILE_xmlEscape(FILE *dest) const;
    
    // estimate mem used by this subtree
    long estimateMemUsage() const;
                        
    // trim array usage down to minimum, recursively
    void trimMemUsage();

    // extract the FilenameNode from a Filename; done here because
    // this class is a friend of Filename
    static FilenameNode *getFilenameNode(Filename const &fn);
};


// get or create super-root node
FilenameNode *getSuperRootNode();


// get a root node, bumping its refct
FilenameNode *getRootNode(char const *fs,
                          int fsLen,
                          bool absolute);


CLOSE_NAMESPACE(FilenameNS)

#endif // FILENAME_CLASS_IMPL_HPP
