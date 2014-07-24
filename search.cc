/**
 * search:
 * find
 * lower_bound
 * upper_bound
 * equal_range
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

// Note the difference btw lower, upper 
// is how to handle the left at the end.
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
    return left;
}

// [lower, upper)
struct pair_t{
public:
    pair_t():lower(0), upper(0){

    }
    pair_t(l, u): lower(l), upper(u) {

    }
private:
    int lower, upper;
};

pair_t equal_range(int *arr, int size, int k)
{
    int lower = lower_bound(arr, size, k);
    if(lower == -1) {
        return pair_t();
    }
    int upper = upper_bound(arr, size, k);
    return pair_t(lower, upper);
}

// supposes that the arr is ascending and of unique elements.
// at any point in the arr, there is at least one side is still ascending and
// k is in either side.
int find_in_rotate(int *arr, int size, int k)
{
    int left = 0, right = size;
    while(left < right) {
        int mid = left + (right - left) / 2;
        if(arr[mid] == k) return mid;

        // acending left
        if(arr[mid] > arr[left]) {

           if(arr[mid] > k && arr[left] < k) {
               right = mid;
           }  else {
               left = mid + 1;
           }

        } // acending right 
        else {
            
            if(k > arr[mid] && k < arr[right - 1]) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }
    }
    return -1;
}

// patch: consider not unique
int find_in_rotate_patched(int *arr, int size, int k)
{
    int left = 0, right = size;
    while(left < right) {
        int mid = left + (right - left) / 2;
        if(arr[mid] == k) return mid;

        int mid_left = mid, mid_right = mid;

        // reduce left and mid_left
        while(left < mid_left && arr[mid_left--] == arr[left++]);

        // reduce right and mid_right
        while(mid_right < right - 1 && arr[mid_right] == arr[right - 1]) {
            mid_right++;
            right--;
        }

        // acending left
        if(arr[mid_left] > arr[left]) {

           if(arr[mid_left] > k && arr[left] < k) {
               right = mid_left;
           }  else {
               left = mid_right + 1;
           }

        } // acending right 
        else if(arr[mid_right] < arr[right]) {
            
            if(k > arr[mid_right] && k < arr[right - 1]) {
                left = mid_right + 1;
            } else {
                right = mid_left;
            }
        }
        else return -1;
    }
    return -1;
}

int find_in_matrix(int **arr, int m, int n, int k)
{
    int left = 0, right = m, upper = 0, bot = n;
    while(left < right || upper < bot) {
        int hmid = left + (right - left) / 2;
        int vmid = upper + (bot - upper) / 2;

        if(arr[hmid][vmid] == k) return hmid * n + vmid;

        if(arr[upper][left] < k && k > arr[hmid][vmid]) {
            right = vmid;
            bot = hmid;
        } else {
            // trivial other 3 cases.
        }
    }
    return -1;
}
