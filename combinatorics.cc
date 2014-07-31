/**
 * The wiki page: http://en.wikipedia.org/wiki/Combinatorics 
 * has listed a lot of interesting stuff to implement. 
 * Implement them in a way of 'yield-return'.
 */
#include "container.hpp"
// "0ab" -> ["0ab", "0aB", "0Ab", "0AB"])
void help(string orig, int idx, string path, vector<string> res)
{
    if(idx == orig.size()) {
        res.push_back(path);
    }

    path += orig[idx];
    help(orig, idx, path, res);

    char a = orig[idx];
    if('0' <= a && a <= '9') {
        return;
    } else if ('a' <= a && a <= 'z') {
        a = a + 26;
    } else {
        a = a - 26;
    }
    path += a;
    help(orig, idx, path, res);
}

// minCoin
// the minimum coins needed to make up n
int minCoin(int n, vector<int> units)
{
    int *arr = new int[n];
    arr[0] = 0;

    for(int i = units[0]; i <= n; ++i) {
        int min = INT_MAX;
        foreach(it, units) {
            if(i - *it > -1 && arr[i - *it] != INT_MAX) {
                if(min > arr[i - *it] + 1) {
                    min = arr[i - *it] + 1;
                }
            }
        }
        arr[i] = min;
    }

    return arr[n];
}

// whether n can be made up
bool minCoin1(int n, vector<int> units)
{
    int *arr = new int[n];
    arr[0] = 1;

    foreach(it, units) {
        for(int i = arr[*it]; i <= n; ++i) {
            arr[i] |= arr[i - *it];
        }
    }

    return arr[n];
}

// fibbonaci heap

// given n, output a binary string without consecutive 1's
// what about 0's

// Write a program to implement Boggle Game.
// You are given a 4x4 matrix of letters and a dictionary, find all the valid
// words in the matrix. Following are the conditions
// 1. If a letter is used, it should not be used again in the same word search
// 2. The word path can be of any direction
// 3. There has to be a path of the letters forming the word( in other words
// all the letters in the word must have to adjacent to one another)
//
// Example:
//
//
// D A T H
// C G O A
// S A T L
// B E D G
//
//
// Some of the Valid words are:
// DATA, HALO, HALT, SAG, BEAT, TOTAL, GLOT, DAG
//
// Not valid words:
// DAGCD ( D cannot be used again)
// DOG ( There is no path between letters)

// NB: DFS all paths is not correct, unless you can concatenate the 
// explored path to the current path ending due to have been explored.

// extend the cur-path only when possible (trie).

// Jump Game:
// Given an array start from the first element and reach the last by jumping.
// The jump length can be at most the value at the current position in the
// array. Optimum result is when u reach the goal in minimum number of jumps.
// For ex:
// Given array A = {2,3,1,1,4}
// First thought: array of max till i, which implies i -> i + max[i]
// NB: this is wrong, because you should jump j -> j + arr[j]
int jump(int *arr, int n)
{
    int idx = 0, steps = 0;
    while(idx < n) {
        int idx_end = idx + arr[idx], max = INT_MIN;
        if(idx_end >= n) {
            ++steps;
            break;
        }
        for(int i = idx + 1; i <= idx_end; ++i) {
            int tmp = i + arr[i];
            if(tmp > max) {
                max = tmp;
            }
        }
        steps += 2;
        idx = max;
    }
    return steps;
}

// More combinatoric: how to exactly reach the last element.
bool run_exact_jump(int *arr, int n, int idx, map<int,bool> &mem)
{
    if(contains(mem, idx)) {
        return mem[idx];
    }
    for(int i = idx; i < n && i < arr[idx] + idx; ++i) {
        if(i + arr[i] == n) {
            mem[idx] = true;
            return true;
        } else if(i + arr[i] < n) {
            if(run_exact_jump(arr, n, i, mem)) {
                mem[idx] = true;
                return true;
            }
        }
    }
    return false;
}

bool exact_jump(int *arr, int n)
{
    map<int, bool> mem;
    return run_exact_jump(arr, n, 0, mem);
}

// N-queen
// 1) permutation
// 2) rotate basic solution
bool run_nqueen(vector<int> &path, set<int> &candidates, int idx)
{
    if(candidates.empty()) return true;

    foreach(it, candidates) {
        bool has_diagnal = false;
        for(int i = 0; i <path.size(); ++i) {
            if(idx - i == *it - path[i]) {
                has_diagnal = true;
                break; // this means diagnal
            }
        }
        if(!has_diagnal) {
            candidates.erase(*it);
            path.push_back(*it);
            if(run_nqueen(path, candidates, idx+1)) {
                return true;
            }
            path.pop_back();
            candidates.insert(*it);
        }
    }

    return false;
}

vector<int> nqueen(int n)
{
    vector<int> path;
    set<int> candidates;
    {
        for(int i = 0; i < n; ++i) {
            candidates.insert(i);
        }
    }
    run_nqueen(path, candidates, 0);
    return path;
}

// Find the all the sequence from Unsorted array.
//
// Example : {2,4,6,8,10,14,11,12,15,7} is the unsorted array. We have to find
// out possible sequences.
// Output would be :
// Seq 1 : {2,4,6,8,10,11,12,15}
// Seq 2 : {2,4,6,8,10,14,15}
//
// or longest increasing sequence.

// Design an algorithm to find the maximum profit. You may complete as many transactions as you like (ie, buy one and sell one share of the stock multiple times). However, you may not engage in multiple transactions at the same time (ie, you must sell the stock before you buy again).

// Design an algorithm to find the maximum profit. You may complete at most two transactions.
