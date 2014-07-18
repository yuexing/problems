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

// Tarjan SCC

// build Line-graph
