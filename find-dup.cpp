/**
 * Find duplicate in an array :
 * - [1, n-1]
 * - n elements
 */
int find_dup1(int *a, int n)
{
    // sulution 1
    for(int i = 0; i < n; ++i) {
        int idx = abs(a[i]);
        if(a[idx] < 0) {
            return idx; 
        } else {
            a[idx] = -a[idx];
        }
    }

    // solution 2
    // add_all - sum(1, n-1)

    // solution 3
    // (1^2^3^4) ^ (1^2^3^3^4) = 3
}

/**
 * Find the only one *non*-duplicate in an array in which other elements are
 * duplicated 
 */
int xor_sum(int *a, int n)
{
    int m = 0;
    for(int i = 0; i < n; ++n) {
        m ^= a[i];
    }
    return m;
}
int find_non_dup1(int *a, int n)
{
    return xor_sum(a, n);
}

// reasoning: xor is commutative, associative
void find_non_dup2(int *a, int n, int *x, int *y)
{
    int m = xor_sum(a, n);
    // find a 1 bit from m to distinguish btw x and y
    // also separate the 'a'
    int bit_one = m & (~(m-1));
    *x = *y = 0;
    for(int i = 0; i < n; ++i) {
        if(a[i] & bit_one) {
            *x ^= a[i]; 
        } else {
            *y ^= a[i];
        }
    }
}
