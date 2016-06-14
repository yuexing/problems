#include <stack>
#include <set>
using namespace std;
/**
 * Functions manipulating binary trees.
 * 
 */

// write a iterator which traverse the tree in a post-order such that the
// consumer can just call operator++ to retrieve the next one.

struct Node
{
    int v;
    Node* left, *right;
};

template<class C, class V> bool contains(C c, V v)
{
    return c.find(v) != c.end();
}

extern void visit(/*nullable*/Node*);

// pre-order
void pre_order(Node *root)
{
    stack<Node*> s;

    while(root || !s.empty()) {
        if(!root) {
            root = s.top();
            s.pop();
        }
        visit(root);
        root = root->left;
        if(root->right) {
            s.push(root->right);
        }
    }
}

// in-order
void in_order(Node *root)
{
    stack<Node*> s;

    while(root || s.empty())
    {
        while(root) {
            visit(root->left);
            s.push(root);
            root = root->left;
        }
        while(!root && !s.empty()) {
            root = s.top();
            s.pop();
            visit(root);
            root = root->right;
        }
    }
}

// post-order
// a exploring set is helpful
void post_order(Node *root)
{
    stack<Node*> s;
    set<Node*> exploring;

    while(1) {
        while(root) {
            visit(root->left);
            s.push(root);
            root = root->left;
        }
        while(!root && !s.empty()) {
            root = s.top();
            // both left and right has been visited
            if(contains(exploring, root)) {
                s.pop();
                visit(root);
                root = NULL;
            } else {
                exploring.insert(root);
                root = root->right;
            }
        }
    }
}

// bounded AVL tree
// basic (symmetric) cases:
//    *   *
//  *   *
//*       *

// red-black tree
// 1. red cannot have red children
// 2. the # of black has to be the same on every path
// 3. the root of the tree is black.
// Also, red-black tree is said to be combination of bst and btree.


// least comment ancestor (syntheses way)
namespace {
struct ret_t {
    bool founda, foundb;
    const Node* lca;

    ret_t(): founda(false),
             foundb(false),
             lca(){}

    ret_t(bool founda, bool foundb, const Node* lca)
      : founda(founda),
        foundb(foundb),
        lca(lca) {}
};

ret_t operator|(const ret_t &a, const ret_t &b)
{
    return ret_t(a.founda | b.founda, a.foundb | b.foundb, NULL);
}

ret_t _find_lca(const Node* root, int a, int b ) 
{
    ret_t rv1 = _find_lca(root->left, a ,b);
    if(rv1.lca) {
        return rv1;
    }
    ret_t rv2 = _find_lca(root->right, a ,b);
    if(rv2.lca) {
        return rv2; 
    }
    // TODO: cond_assert
    ret_t rv = rv1 | rv2;
    if(rv.founda && rv.foundb) {
        rv.lca = root;
    } else if(root->v == a) {
        rv.founda = true;
    } else if(root->v == b) {
        rv.foundb = true;
    }
    return rv;
}

const Node *find_lca(const Node* root, int a, int b)
{
    ret_t rv = _find_lca(root, a, b);
    return rv.lca;
}

// given an interval, print all nodes in the tree within that interval.
// NB: the interval is made of double.
extern bool compare_int_double(int, double);
extern void print_range(const Node *root, const Node *begin, const Node *end);
namespace {
struct interval_t{
    double x, y;
};

void find_delim(const Node *root, double x, bool find_begin, const Node *last)
{
    if(!root) return;
    int rv = compare_int_double(root->v, x);
    if(!rv) {
        last = root;
    } else if(rv < 0) {
        if(!find_begin) {
            last = root;
        }
        find_delim(root->right, x, find_begin, last);
    } else {
        if(find_begin) {
            last = root;
        }
        find_delim(root->left, x, find_begin, last);
    }
}

void print_range(const Node *root, const interval_t &i)
{
    Node *begin = NULL, *end = NULL;
    find_delim(root, i.x, true, begin);
    if(begin == NULL) return;
    find_delim(root, i.y, false, end);
    if(end == NULL) return;
    print_range(root, begin, end);
}
}

// Find height of a tree without using recursion


struct interval_node_t
{
    int i, j, largest_j;
    interval_node_t *left, *right;
};

// The tree is sort by i. largest_j is the largest j in the subtree 
// rooted at the node.
// print out all the intervals containing the n
void print_intervals(interval_node_t *r, int n)
{
    if(!r) {
        return;
    }

    if(r->i <= n) {
        if(r->j >= n) {
            //print
        } 
        if(r->largest_j >= n) {
            print_intervals(r->left, n);
            print_intervals(r->right, n);
        }
    } else {
        print_intervals(r->left, n);
    }
}
