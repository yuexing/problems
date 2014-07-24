/**
 * All the sorting:
 * select-sort
 * insert-sort
 * merge-sort
 * quick-sort
 * radix-sort : also refered to as counting sort. but counting sort is useful
 * for multi-factor sorting when first several factors are already sorted.
 */

void select_sort(int *arr, int size)
{
    for(int i = 0; i < size; ++i) {
        int min = i;
        for(int j = i+1; j < size; ++j) {
            if( arr[j] < arr[min] ) {
                min = j;
            }
        }
        int tmp = arr[i]; arr[i] = arr[min]; arr[min] = arr[i];
    }
}

void insert_sort(int *arr, int size)
{
    for(int i = 1; i < size; ++i) {
        int cur = arr[i], j = i - 1;
        while(j > -1 && arr[j] > cur) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j+1] = cur;
    }
}

void merge_sub(int *arr, int left, int right)
{
    if(left >= right) return;

    int mid = left + (right - left)/2;
    merge_sub(arr, left, mid);
    merge_sub(arr, mid, right);
    
    int *tmp = new int[right - left];
    int i = left, j = mid + 1, k = 0;
    while(i < mid && j < right) {
        if(arr[i] <= arr[j]) {
            tmp[k++] = arr[i++];
        } else {
            tmp[k++] = arr[j++];
        }
    }
    while(i < mid) tmp[k++] = arr[i++];
    while(j < right) tmp[k++] = arr[j++];

    i = left, k = 0;
    while(i < right) {
        arr[i++] = tmp[k++];
    }
    delete[] tmp;
}

// 1) divide into two halves, sort and merge
void merge_sort(int *arr, int size)
{
    merge_sub(arr, 0, size);
}

// All the places i has walked through are less than 
// or equal to pivot.
int split_arr(int *arr, int left, int right)
{
    int pivot = arr[left];
    int i = left + 1, j = right - 1;
    while(i < j) {
        while(arr[j] > pivot) --j;
        while(i < j && arr[i] <= pivot) ++i;

        if( i < j ) {
            int tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            ++i; --j;
        }
    }
    int tmp = arr[j];
    arr[j] = arr[left];
    arr[left] = tmp;
    return j;
}

void quick_sub(int *arr, int left, int right)
{
    if(left >= right) return;

    int pidx = split_arr(arr, left, right);
    quick_sub(arr, left, pidx);
    quick_sub(arr, pidx+1, right);
}

// each loop get a pivot to split the array into two
// quick sort on each part
void quick_sort(int *arr, int size)
{
    quick_sub(arr, 0, size);
}

void filter_min_down(int *arr, int size)
{
    int i = 0;
    while(i < size) {
        int left = i * 2 + 1, right = i * 2 + 2, max = -1;
        if(left < size && right < size) {
            max = (arr[left] > arr[right]) ? left : right;
        } else if(left < size) {
            max = left;
        } else return;

        if(arr[max] > arr[i]) {
            int tmp = arr[max];
            arr[max] = arr[i];
            arr[i] = tmp;
        } else {
            return;
        }
        i = max;
    }
}

void de_max(int *arr, int size)
{
    int tmp = arr[0];
    arr[0] = arr[size - 1];
    arr[size - 1] = tmp;

    filter_min_down(arr, size - 1);
}

void filter_max_up(int *arr, int i)
{
    while(i > 0) {
        int c = i/2;
        if(arr[c] < arr[i]) {
            int tmp = arr[i];
            arr[i] = arr[c];
            arr[c] = tmp;
        } else {
            return;
        }
        i = c;
    }
}

void build_max_heap(int *arr, int size)
{
    for(int i = 1; i < size; ++i) {
        filter_max_up(arr, i);
    }
}

// 1) build the max heap
// 2) pop the front and put it at the end
void heap_sort(int *arr, int size)
{
    build_max_heap(arr, size);
    for(int i = 0; i < size; ++i) {
        de_max(arr, size - i);
    }
}

int max(int *arr, int size)
{
    int max = 0;
    for(int i = 1; i < size; ++i) {
        if(arr[i] > arr[max]) {
            max = i;
        }
    }
    return arr[max];
}

int extract(int n, int e)
{
    return (n / (e/10)) % 10;
}

void count_sort(int *arr, int size, int e, int range)
{
    int counts = new int[range];
    int outs = new int[size];
    for(int i = 0; i < size; ++i) {
        counts[extract(arr[i], e)]++;
        outs[i] = arr[i];
    }
    // the accumulated number 
    for(int i = 1; i < range; ++i) {
        counts[i] += counts[i-1];
    }
    for(int i = size-1; i > 0; --i) {
        arr[counts[extract(arr[i], e)] - 1] = arr[i];
        --counts[extract(arr[i], e)];
    }
    delete[] outs;
    delete[] counts;
}

// counting sort
// the complexity: log_range(max) * (size + range)
void radix_sort(int *arr, int size, int range)
{
    int max = max(arr, size); 
    for(e = 10; m/10; e *= 10) {
        count_sort(arr, size, e, range);
    }
}
