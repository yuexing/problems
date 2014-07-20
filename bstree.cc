/**
 * Functions manipulating binary trees.
 * 
 */

// write a iterator which traverse the tree in a post-order such that the
// consumer can just call operator++ to retrieve the next one.

struct Node
{
    Node* left, right;
};

extern void visit(/*nullable*/Node*);

// pre-order
void pre_order(Node *root)
{
    stack<Node*> s;

    while(root || !s.empty()) {
        if(!root) {
            root = s.pop();
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
            root = s.pop();
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
            root = s.pop();
            // both left and right has been visited
            if(contains(exploring, root)) {
                visit(root);
                root = NULL;
            } else {
                exploring.insert(root);
                s.push(root);
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
