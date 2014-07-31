#include "scoped-ptr.hpp"
#include "container.hpp"

// test prime
// according to the prime number theorem, the possibility of n being a prime
// is 1/lg(n).
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
// Given an array, find the subarray (consecutive) with largest sum.
int find_largest_sum(int *arr, int size)
{
    scoped_array_t<int> sum(new int[size]);
    sum[0] = arr[0];
    for(int i = 1; i < size; ++i) {
        sum[i] = sum[i-1] + arr[i];
    }
    // get the largest difference
    int min = sum[0], max_sum = sum[0];
    for(int i = 1; i < size; ++i) {
        if(sum[i] - min > max_sum) {
            max_sum = sum[i] - min;
        }
        if(min > sum[i]) {
            min = sum[i];
        }
    }
}

void extract_digits(int num, vector<char> digits)
{
    while(num) {
        digits.push_back(num % 10);
        num /= 10;
    }
    std::reverse(digits.begin(), digits.end());
}

int number(vector<char> digits)
{
    int sum = 0;
    foreach(it, digits) {
        int temp = sum * 10 + *it;
        if(temp < sum) {
            return -1;  //overflow
        }
    }
    return sum;
}

// Given a number find the next bigger number with the same digits otherwise return
// -1. E.g. 5678 -> 8754, 8765 -> -1
// next_smaller: is to find the begin of the ascending going from the end.
int next_bigger(int num)
{
    vector<char> digits;
    extract_digits(num, digits);
    int i = digits.size() - 1;
    for(; i > 0; --i) {
        if(digits[i] > digits[i-1]) { // find the begin of the descending going from the end.
            break;
        }
    }
    // the smallest digit in [i,]
    // followed by other digits in ascending order.
    return number(digits);
}

// Find the biggest number using the same digits from num, otherwise return -1.
int rearrange(int num)
{
    vector<char> digits;
    extract_digits(num, digits);
    std::sort(digits.begin(), digits.end());
    std::reverse(digits.begin(), digits.end());

    // 
    vector<char> intmax_digits;
    extract_digits(INT_MAX, intmax_digits);

    if(digits.size() == intmax_digits.size()) {
    } else {
        int sum = number(digits);
        if(sum == num) {
            return -1;
        }
        return sum;
    }
    
}

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

// Given an array, find out if there exist a subarray such that its sum is
// X.
bool findX1(int *arr, int n, int x)
{
    scoped_array_t<int> sarr(new int[n]);
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
bool findX2(int *arr, int n, int x)
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
// 1) sort, then greedily assign the number one by one to each set;
// 2) dyn, p(sum, n) = p(sum, n - 1) or p(sum - arr[n], n - 1)
// 3) similar to huffman.

// Another kind of partition: Birthday_problem#Partition_problem


// Replace element of an Array with nearest bigger number at right side of the
// Array in O(n) 
void replace_right_bigger(int *arr, int n)
{
    int *right_bigger = new int[n];
    
    stack<int> s;
    s.push(arr[n-1]);

    for(int i = n - 2; i > -1; ++i) {
        while(!s.empty()) {
            if(s.top() <= arr[i]) {
                s.pop();
            } 
        }
        if(s.empty()) {
            right_bigger[i] = arr[i];
        } else {
            right_bigger[i] = s.top();
        }
        s.push(arr[i]);
    }
}

// A similar one is to trap rain water
// get-left-biggest -> idx
// get-right-biggest -> idx
int total_trap_water(int *arr, int n)
{
    int max = arr[0];
    int *maxL = new int[n];
    maxL[0] = -1;
    for(int i = 1; i < n; ++i) {
        maxL[i] = max;
        if(arr[i] > max) {
            max = arr[i];
        }
    }
    max = arr[n-1];
    int area = 0;
    for(int i = n -2; n > -1; --n) {
        int hight = std::min(maxL[i], max);
        if(hight > arr[i]) {
           area += (hight - arr[i]); 
        } else {
            /* most come to here */
        }
    }
    return area;
}

// foreach i
// get left next smaller
// get right next smaller
//
// can this be reduced to one loop?
int hist_area(int *arr, int n)
{
    return -1;
}
