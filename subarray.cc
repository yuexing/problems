#include <utility>
using namespace std;
// Given an array, find the subarray (consecutive) with largest sum.
void find_largest_sum(int *arr, int size)
{
    int left = 0, right = 0, sum = arr[0];
    int max_left = left, max_right = right, max = sum;

    for(int i = 1; i < size; ++i) {
        sum += arr[i];
        if(sum < arr[i]) {
            left = right = i;
            sum = arr[i];
        } 
        if(sum > max) {
            max_left = left;
            max_right = right;
            max = sum;
        }
    }

    // between max_left and max_right
}

extern int get_next_0ele(int *arr, int size, int start);

struct arr_wrapper
{
    int* arr;
    int  size;

    arr_wrapper(int* arr, int size)
        : arr(arr),
          size(size) {}

    struct iterator {
        iterator(int left, int right, int *arr, int size)
            :p(left, right),
             arr(arr),
             size(size){}

        iterator& operator++(){
            p = make_pair(p.second, get_next_0ele(arr, size, p.second));
        }

        pair<int,int>& operator*() {
            return p;
        }

        bool operator==(const iterator &o) {
            return o.arr == arr && o.size == size && o.p == p;
        }

        pair<int, int> p;
        int *arr;
        int size;
    };

    iterator begin() {
        return iterator(0, get_next_0ele(arr, size, 0), arr, size);
    }

    iterator end() {
        return iterator(size, size, arr, size);
    }
};
// Given an array, find the subarray with largest product.
// anything times 0 is 0
// positive -> negative -> positive
void find_largest_product(int *arr, int size)
{
   // segment the array by zero
   // find largest product on those subarray.
}
