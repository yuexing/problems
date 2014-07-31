/**
 * Algorithms related to graphs.
 */

struct gnode_t;
vector<gnode_t*> get_neighbors(gnode_t*);
vector<gnode_t*> unvisited(vector<gnode_t*>, set<gnode_t*>);
int get_dist(gnode_t* from, gnode_t* to);

// shortest path

// Djikstra
// each loop, relax the unvisited neighbors.
// alternatively, we can relax on all neighbors, which is slow (but correct
// for graph with negative edges) and subject to negative cycles.
// Q: how to detect negative cycles?
// record the # of times a node is pushed into the worklist.
int djikstra(gnode_t* src, gnode_t* dest)
{
    priority_queue<gnode_t*> worklist;
    set<gnode_t*> visited;
    map<gnode_t*, int> dists;
    dists[src] = 0;
    worklist.push(src);

    while(!empty(worklist)) {
        gnode_t* n = worklist.pop();
        visited.insert(n);
        if(n == dest) {
            return dists[dest];
        }
        foreach(it, unvisited(get_neighbors(n), visited)) {
            int proposed = dists[n] + get_dist(n, *it);
            if(!contains(dists, *it)){
                dists[*it] = proposed;
                worklist.push(*it);
            } else if(dists[*it] > proposed) {
                dists[*it] = proposed;
                worklist.filter_up(*it);
            }
        }
    }
}

// Bellman-Ford
// slow-version of Djikstra.
int bellmanFord(int **dists, int n, int m, int src, int dest)
{
    int **mem = new int*[m];
    for(int i = 0; i < m + 1; ++i) {
        mem[i] = new int[n];
    }

    // init all as INT_MAX
    mem[0][src] = 0;

    // run
    for(int i = 1; i <= m; ++i) {
        for(int j = 0; j < n; ++j) {
            // look at all the adjacent
            int min = INT_MAX;
            for(int k = 0; k < n; ++k) {
                if(mem[i-1][k] != INT_MAX && dists[k][j] != INT_MAX) {
                    int temp = mem[i-1][k] + dists[k][j];
                    if(temp < min) {
                        min = temp
                    }
                }
            }
            mem[i][j] = min;
        }
    }

    return mem[m][dest];
}

// Floyd Warshall
// improving an estimate on the shortest path between two vertices, until the
// estimate is optimal.
void floyd(int **dists, int n, int **res)
{
    // initialize res with dists
    // run
    for(int k = 0; k < n; ++k) {
        for(int i = 0; i < n; ++i) {
            for(int j = 0; j < n; ++j) {
                // need a copy?
                // NO!
                res[i][j] = min(res[i][k] + res[k][j], res[i][j]);
            }
        }
    }
}

// Tarjan SCC

// build Line-graph
