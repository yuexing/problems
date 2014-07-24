#include <set>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;
#include <stdlib.h>
#include "container.hpp"
#include "debug.hpp"

struct Username
{

static int get_idx(const string &s, const string &b) {
    return 0;
}

static string newMember(vector<string> existing, string name)
{
set<int> sorted;
vector<int> idxes;

bool existed = false;
foreach(it, existing) {
    int n = get_idx(*it, name);
    if(n == 0) {
        existed = true;
    } else if(n > 0) {
        sorted.insert(n);
    }
}
if(!existed) {
    return name;
}

foreach(it, sorted) {
    idxes.push_back(*it);
}

string ret(name);
if(idxes[0] != 1) {
    ret += 1;
} else {
    int n = find_non_existing(idxes);
    ret += n;
}
return ret;
}

// assume that arr is sorted.
// This will find first non-existing.
static int find_non_existing(vector<int> arr)
{
int left = 0, right = arr.size();
while(left < right) {
    int lcnt = (right - left) / 2;
    int mid = left + lcnt;
    // look at mid
    if(mid > 0) {
        if(arr[mid] != arr[mid - 1] + 1) {
            return arr[mid-1] - 1;
        }
    }
    if(mid + 1 < arr.size()) {
        if(arr[mid] != arr[mid + 1] - 1) {
            return arr[mid] + 1;
        }
    }
    // looking at left or right
    // change this can change to find last-non-existing
    if(arr[mid] - arr[left] > lcnt) {
        right = mid;
    } else {
        cond_assert(arr[mid] - arr[left] == lcnt);
        left = mid + 1;
    }
}
}
};

int main()
{
return 0;
}
