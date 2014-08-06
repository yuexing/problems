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

// x ^ (x << 1)
// the number will loop

// the C version of xorshift
uint32_t x, y, z, w;
 
uint32_t xor128(void) {
    uint32_t t;
 
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    return w = w ^ (w >> 19) ^ t ^ (t >> 8);
}

// The example implementation of libc
// linear congruential generator
int myrand(void) {
    static unsigned long next = 1;
    next = next * 1103515245 + 12345;
    return((unsigned)(next/65536) % 32768);
}

#include <iostream>
#include <fstream>
// read from /dev/random, /dev/urandom
struct rand_t
{
    rand_t(): ifs("/dev/random", ifstream::in)
    {
    }
    int operator()()
    {
        uint32_t v;
        ifs.read((char*)&v, 4);
        return v % 32768;
    }
private:
    ifstream ifs;
};

// run a hash function against a frame of a video stream from an unpredictable source

// frequency test
// serial test: 
// poker test: 
// gap test
