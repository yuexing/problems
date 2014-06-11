/**
 * search:
 * find
 * lower_bound
 * upper_bound
 */

int find(int *arr, int size, int k)
{
int left = 0, right = size;
while(left < right) {
    int mid = left + (right - left) / 2;
    if(arr[mid] == k ) return mid;
    else if (arr[mid] < k) left = mid + 1;
    else right = mid;
}
return -1;
}

int lower_bound(int *arr, int size, int k)
{
int left = 0, right = size;
while(left < right) {
    int mid = left + (right - left) / 2;
    if(arr[mid] >= k) right = mid;
    else left = mid + 1;
}
if(left == size || arr[left] != k) return -1; 
return left;
}

int upper_bound(int *arr, int size, int k)
{
int left = 0, right = size;
while(left < right) {
    int mid = left + (right - left) / 2;
    if(arr[mid] <= k) left = mid + 1;
    else right = mid;
}
if(left == 0 || arr[left - 1] != k) return -1; 
return left - 1;
}

int find_in_rotate(int *arr, int size, int k)
{
    return -1;
}

int find_in_matrix(int **arr, int m, int n, int k)
{
    return -1;
}
