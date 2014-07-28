#include "container.hpp"

// follow[i][j] : i follows j

// count degree
int find_influencer1(int **follow, int n)
{
    int *degreeIn = new int[n], degreeOut = new int[n];
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
            dfs(follow, n, j, visited, root);
        }
    }
    if(!hasFollow) {
        roots.insert(j);
    }
}

// do DFS search
int find_influencer2(int *follow, int n)
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
