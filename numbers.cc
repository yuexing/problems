#include "scoped-ptr.hpp"
#include "container.hpp"

// test prime
bool testPrime(int n)
{
    if(!(n % 2)) return false;
    if(!(n % 3)) return false;
    // now only consider the 6k+/-1
    for(int k = 1; ; ++k) {
        int base = 6 * k, a = base - 1, b = base + 1;
        if(a * a > n) return true;
        if(!(n % a)) return false;
        if(b * b > n) return true;
        if(!(n % b)) return false;
    }
    return true;
}

// find as many primes as up to n
void getPrimes(int n, vector<int> primes)
{
    primes.push_back(2);
    for(int i = 3; i <= n; ++i) {
        bool divisable = false;
        foreach(it, primes) {
            if(!(i % *it)) {
                divisable = true;
            }
        }
        if(!divisable) {
            primes.push_back(i);
        }
    }
}

// Find all the (prime) factors
void getFactors(int n, set<int> factors)
{
    vector<int> primes;
    getPrimes(n, primes);

    // recursively run getFactors? Yes.
    // run getSubsets on primes? No. because a prime can exist multiple times
    // in a factor.
}

// subsets of a set.
void getSubsets(vector<int> orig, vector<vector<int> > subsets)
{
    vector<int> base;
    subsets.push_back(base);
    base.push_back(orig[0]);
    subsets.push_back(base);

    for(int i = 0; i < orig.size(); ++i) {
        int sz = subsets.size();
        for(int j = 0; j < sz; ++j) {
            base = subsets[j];
            base.push_back(orig[i]);
            subsets.push_back(base);
        }
    }
}

// Given a number find a bigger number with the same digits otherwise return
// -1. E.g. 5678 -> 8754, 8765 -> -1

// Given an array of integers (can be both positive and negative), find 3
// which multiply to give the largest product

//  Given an array of integers, find out a triple of integers such that
//  they form a triangle.
// 1) sort
// 2) i, j find k, increasing k while increasing j.

// Given an array, map(i) -> arr[0]..*..arr[i-1]*arr[i]..*..*
// assume there is no overflow:
// 1) if there is only one 0;
// 2) if there are more than one 0;
// 3) otherwise

// Given an array, find the subarray with largest product.

// Given an array, find out if there exist a subarray such that its sum is
// X.
bool findX1(int *arr, int n, int x)
{
    scoped_array_t sarr(new int[n]);
    set<int> looking_for;

    int sum = 0;
    for(int i = 0; i < n; ++i)
    {
        sum += arr[i];
        sarr[i] = sum; 
        if(contains(looking_for, sum)) {
            return true;
        }
        looking_for.insert(sum + x);
    }
    return false;
}

// assume the array only has nonnegative numbers
bool findX1(int *arr, int n, int x)
{
    int sum = arr[0], start = 0;
    for(int i = 1; i < n; ++i) {
        sum += arr[i];
        while(sum > x ) {
           sum -= arr[start++]; 
        }
        if(sum == x) {
            return true;
        }
    }
    return false;
}

// How to optimally divide an array into two subarrays so that sum of elements
// in both are same.

// if consecutive numbers, then almost the same as findX
// otherwise, a partition problem.
