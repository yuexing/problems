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

