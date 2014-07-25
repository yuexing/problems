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

// Write code to implement huffman coding of a string

// dist btw words in a text.
static vector<string> words;
// NB: the \p list1,\p  list2 stores the positions
// [1,4,7,9]
// [3,6,11,15]
int find_minDist(vector<int> list1, vector<int> list2)
{
    int min = words.size();
    int i = 0, j = 0;
    for(; i < list1.size() && j < list2.size(); )
    {
        int dist = abs(list1[i] - list2[j]);
        if(list1[i] < list2[j]) {
            ++i;
        } else {
            ++j;
        }
        if(dist < min) {
            min = dist;
        }
    }
    // take care about the last one.
    if(i == list1.size()) {

    } else if (j == list2.size()) {

    }
    return min;
}

// what if iterating just on the text
// no duplicates is allowed here.
int find_minDist2(vector<string> text, string word1, string word2)
{
    int loc1 = -1, loc2 = -1, min = text.size();
    for(int i = 0; i < text.size(); ++i) {
        if(text[i] == word1) {
            loc1 = i;
        } else if(text[i] == word2) {
            loc2 = i;
        } else continue;

        if(loc1 != -1 && loc2 != -1) {
            int dist = abs(loc1 - loc2);
            if( dist < min) {
                min = dist; 
            }
        }
    }
    return min;
}

// what if we have bunch of words to look for
// and also allow duplication for the tofind
// NB: also referred to as minWindow.
int find_minDist3(vector<string> text, vector<string> tofind)
{
    map<string, int> needs;
    foreach(it, tofind) {
        ++needs[*it];
    }

    map<string, int> founds;
    queue<int> found_locs;
    int count = 0, begin = 0, min = text.size();
    for(int i = 0; i < text.size(); ++i) {
        if(!contains(needs, text[i])) {
            continue;
        }
        ++founds[*it];
        found_locs.push(i);
        if(founds[*it] < needs[*it]) {
            ++count;
        }
        if(count == tofind.size()) {
            // pop out if possible
            do {
                int idx = found_locs.front();
                if(founds[idx] > needs[*it]) {
                    begin = idx;
                    found_locs.pop();
                } else {
                    break;
                }
            } while(1);
            // now update the min
            int dist = i - begin;
            if(dist < min) {
                min = dist;
            }
        }
    }
    return min;
}

// implement path simplification

// implement cd

// print all palindromes of size k possible from given alphabet set.
// eg alphabet set : {a,e,i,o,u}
// print all palindromes of size say 10.
