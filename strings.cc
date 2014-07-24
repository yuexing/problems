#include "container.hpp"
// recursive intuition:
// [i, j] = 1 + [i-1, j-1];         if p[i] == q[j]
//          max([i-1, j], [i, j-1]);otherwise
// bottom-up:
// 1) arr[2][q.size()]
int LCS(string p, string q)
{
    return 0;
}

// [i, j] = 1 + [i-1, j-1]; if p[i] == q[j]
//        = 0;              otherwise
// or:
// merge the array of suffixes. 
// or:
// LCA of the suffix-tree.
int LCStr(string p, string q)
{
    return 0;
}

// [i, j] = 0 + [i-1, j-1];     equal
//        = 1 + [i-1, j];       delete
//        = 1 + [i, j-1];       add
// take the max.
int edit_dist(string p, string q)
{
    return 0;
}

// KMP
// partial table (failure function) : T: records the length of the matched real prefix 
// q: ABCDABD
// once when there is a mismatch, decides start from which part of q.
bool strstr(string p, string q)
{
    // calculate partial table T
    int *T = new int[q.size()];
    T[0]= 0;
    for(int i = 1, j = 0; i < q.size(); ++i) {
        if(q[i] == q[j]) {
            T[i] = ++j;
        } else {
            do {
                j = T[j];
            } while(j != 0 && q[i] != q[j]);

            if(q[i] == q[j]) {
                T[i] = ++j;
            } else {
                T[i] = 0;
            }
        }
    }

    // strstr
    for(int i = 0, j = 0; i < p.size(); ++i)
    {
        if(p[i] == q[j]) {
            if(j == q.size() - 1) {
                return true;
            } else {
                ++j;
            }
        } else {
            do {
                j = T[j];
            } while(j != 0 && q[i] != q[j]);
        }
    }

    return false;
}

// text justification routine
// Greedy solution:
// counter case:
// [aaa bb cc ddddd] 6
// Dynamic solution:
// 1) line-cost: lc[i, j] the cost for puting words [i, j) in one line
// 2) cost: c[i] = c[j-1] + lc[j, i]

// Given two (dictionary) words as Strings, determine if they are isomorphic. Two words are called isomorphic 
// if the letters in one word can be remapped to get the second word. 

// given a larger file(something like that), report the top k most frequent
// words.
// The intuition is to count and minHeap
// to combine them, need a trie and a minHeap

#define SIZE 26

struct trieAndHeap
{
    struct node_t
    {
        bool is_end;
        int count;  
        int heapIdx;

        node_t *children[SIZE];

        node_t()
            : is_end(false),
              count(0),
              heapIdx(-1) {
                  memset(children, SIZE * sizeof(node_t*), 0);
              }
    };

    static cmp_node_t(const node_t& n1, const node_t& n2)
    {
        return 0;
    }

    struct minHeap
    {
        minHeap(int capacity)
            : capacity(capacity), 
              size(0),
              arr(new node_t*[capacity]) {
        }

        void addOrUpdate(node_t *n)
        {
            if(n.heapIdx != -1) {
                filterDown(n);
            }

            if(size == capacity) {
                if(n.count > arr[0].count) {
                    arr[0].heapIdx = -1;
                    arr[0] = n;
                    n.heapIdx = 0;
                    filterDown(n);
                }
            } else {
                arr[size] = n;
                n.heapIdx = size++;
                filterUp(n);
            }
        }
    private:
        int capacity;
        int size;
        node_t **arr;

        void filterDown(node_t *n)
        {
        }

        void filterUp(node_t *n)
        {
        }
    };

    int insert(const string &s)
    {
        return insert(root, s.c_str());
    }

    void print()
    {
    }
private:
    node_t *root;
    minHeap heap;

    int insert(node_t *root, const char* s)
    {
        node_t *n = root->children[s[0]];
        if(!n) {
            n = root->children[s[0]] = new node_t;
        }
        if(s[1] == '\0') {
            n.is_end = true;
            n.count++;
            
            // also put into the minHeap
            heap.addOrUpdate(n);
        } else {
            insert(n, s + 1);
        }
    }
};

// Finding the Minimum Window in S which Contains All Elements from T
// example: S "acbbaca" T "aba"
int minWindow(const string& p, const string &q)
{
    int needs[SIZE] = { 0 };
    foreach(it, q) {
        ++needs[*it - 'a'];
    }

    int founds[SIZE] = { 0 };
    int count = 0;
    int min = 0;
    for(int i = 0; i < p.size(); ++i)
    {
        int idx = p[i] - 'a';
        if(needs[idx] == 0) {
            continue;
        }

        found[idx]++;
        // hold the constraint
        if(founds[idx] <= needs[idx]) {
            count++;
        } 

        // based on the constrait
        if(count == q.size()) {
            // advance the begining if possible
            // then update the global minWindow
        }
    }
    return min;
}

// Write code to implement huffman coding of a string
