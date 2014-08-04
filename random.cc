#include "container.hpp"
// suppose we already have a random-number-generator which can generate 
// numbers following the universal distribution.
// create a random-number-generater that can generate numbers with weights.

// The following way is somewhat as the same as:
// int r = universal_gen(0, sum-of-all-weights);
// foreach(weights, it) {
//      if(r < it->second) {
//          return it->first;
//      }
//      r -= it->second;
// }
//[start, end)
extern int universal_gen(int start, int end);

// number -> # of times it should appear in 10 times
#define BUF_SIZE 10
static int buf[BUF_SIZE];
int rng_init(map<int, int> weights)
{
   int i = 0;
   foreach(it, weights) {
    for(int j = 0; j < it->second; ++j) {
        buf[i++] = it->first;
    }
   }
   // assert
}

int rng()
{
    int r = universal_gen(0, 9);
    return buf[r];
}

// given the following constraints, generate numbers:
// 1) sum to x;
// 2) each is in the range of [min, max)
