/**
 * All the sorting:
 * select-sort
 * insert-sort
 * merge-sort
 * quick-sort
 * radix-sort
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

void quick_sort(int *arr, int size)
{
    quick_sub(arr, 0, size);
}

// how?
void radix_sort(int *arr, int size)
{

}
