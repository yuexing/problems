#include <vector>
using namespace std;

struct point {
    int x, y;

    point(): x(-1),
             y(-1){}
};

// return the dist^2 actually.
int dist(const point& p, const point& q)
{
    return (p.x-q.x) * (p.x-q.x) + (p.y-q.y) * (p.y-q.y);
}

bool point_compare_lt(const point&p, const point& q)
{
    if(p.x != q.x) {
        return p.x < q.x;
    }
    return p.y < q.y;
}

typedef vector<point>::iterator it_t;

struct ret_t {
    point p, q;
    int d;

    ret_t(): p(),
             q(),
             d(-1) {}
};

bool ret_compare_lt(const ret_t& rv1, const ret_t& rv2)
{
    if(rv1.d == -1) {
        return false;
    } else if(rv2.d == -1) {
        return true;
    }
    return rv1.d < rv2.d;
}

// select the points which is within d from the mid point.
// for each pi, reverse_iterates pj < pi where j<i and yi-yj < d.
// According to the thorem, there are at most 7 such points.
ret_t strip(it_t begin, it_t end, int mid, int d)
{
    return ret_t();
}

// sorted arr
ret_t closest_pair(it_t begin, it_t end, int size)
{
    if(size <= 1) return ret_t();

    int mid = size/2;
    ret_t rv1 = closest_pair(begin, begin + mid, mid);
    ret_t rv2 = closest_pair(begin + mid + 1, end, size - mid);
    ret_t rv = ret_compare_lt(rv1, rv2)? rv1: rv2;

    vector<point> stripped;
    return strip(begin, end, mid, rv.d);
}

// 
void closest_pair(vector<point> arr)
{
    // sort by x
    std::sort(arr.begin(), arr.end(), point_compare_lt);
    // 
    closest_pair(arr.begin(), arr.end(), arr.size());
}

