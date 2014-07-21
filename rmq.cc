// Range Minimum Query(RMQ)
// given an index interval, return the index for the minimum element within the range.

#include <math.h>

// Trival pre-processing
#define N 50
int arr[N];

namespace trival{
int mem[N][N];

void preprocess()
{
    static bool processed = false;
    if(processed) {
        return;
    }
    processed = true;
    for(int i = 0; i < N; ++i) {
        mem[i][i] = i;
        for(int j = i+1; j < N; ++j) {
            if(arr[mem[i][j-1]] < arr[j]) {
                mem[i][j] = mem[i][j-1];
            } else {
                mem[i][j] = j;
            }
        }
    }
}
int rmq(int i, int j)
{
    preprocess();
    // cond_assert(i <= j);
    return mem[i][j];
}
}

// <N, sqrt(N)>
namespace another{
const int nsection = ceil(sqrt(N));
int *mem/*[nsection]*/;

void preprocess()
{
    static bool processed = false;
    if(processed) {
        return;
    }
    processed = true;
    for(int i = 0; i < nsection; ++i) {
        mem[i] = i * nsection;
        for(int j = i*nsection + 1; j < N && j < (i+1) * nsection; ++j) {
            if(arr[j] < arr[mem[i]]) {
                mem[i] = j;
            }
        }
    }
}

int rmq(int i, int j)
{
    int start = i/nsection, end = j/nsection;
    int min = i;
    for(int k = i+1; k < start * nsection; ++k) {
    }
    for(int k = start; k < end; ++k){
    }
    for(int k = end*nsection; k < j; ++k) {
    }
}
}

// sparse table
// log(j-i+1)
namespace sparse_table {
#define TABLE_SIZE 6

// mem[i][j]: starting at i the minimum with length 2^i
int mem[N][TABLE_SIZE];

void preprocess()
{
    static bool processed = false;
    if(processed) {
        return;
    }

    for(int j = 0; j < TABLE_SIZE; ++j) {
        for(int i = 0; i + (1 << j) - 1 < N; ++i) {
            int temp = i + (1 << (j-1));
            if(arr[mem[i][j-1]] < arr[mem[temp][j-1]]) {
                mem[i][j] = mem[i][j-1];
            } else {
                mem[i][j] = mem[temp][j-1];
            }
        }
    }
}

int rmq(int i, int j)
{
}
};

namespace segment_tree {
};
