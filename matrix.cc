#include "container.hpp"

// follow[i][j] : i follows j

// count degree
int find_influencer1(int **follow, int n)
{
    int *degreeIn = new int[n], *degreeOut = new int[n];
    for(int i = 0; i < n; ++i) {
        for(int j = 0; j < n; ++j) {
            if(follow[i][j]) {
                ++degreeOut[i];
                ++degreeIn[j];
            }
        }
    }
    for(int i = 0; i < n; ++i) {
        if(degreeIn[i] == n-1 && degreeOut[i] == 0) {
            return i;
        }
    }
    return -1;
}

void dfs(int **follow, int n, int i, set<int> &visited, set<int> &roots)
{
    if(!visited.insert(i).second) {
        return;
    }
    bool hasFollow = false;
    for(int j = 0; j < n; ++j) {
        if(follow[i][j]) {
            hasFollow = true;
            dfs(follow, n, j, visited, roots);
        }
    }
    if(!hasFollow) {
        roots.insert(i);
    }
}

// do DFS search
int find_influencer2(int **follow, int n)
{
    set<int> visited;
    set<int> roots;
    for(int i = 0; i < n; ++i) {
        if(contains(visited, i)) {
            continue;
        }
        dfs(follow, n, i, visited, roots);
    }

    // check roots now, trivial
    return -1;
}

// rotate a matrix by 90 degree

// Given a board of black and white, find the min/max black rectangle
int find_max(int **m)
{
    // thinking about 2 pair of matrix, the first one use height/left
    // the second one use width/right
    // update height/left:
    // if mm[x,y] == 0, height[x,y] = 0, left[x,y] = 0
    // otherwise,       height[x,y] = height[x-1,y] + 1, left[x,y] = min(left[x-1,y], left[x, y-1])
    return -1;
}

// find the min rectangle containing all the black.
int find_min(int **mm)
{
    // on the way, update the 4 variables.
    int minx, miny, maxx, maxy;
    return (maxx - minx + 1) * (maxy - miny + 1);
}
