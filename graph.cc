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
// Q: it cannot work correctly with negative path?
// A: because the visited set won't be visited again even though there's a less expensive path.
// Q: how to detect negative cycles? since we relax on its neighbors which is
// A: no more than n, then if a node is pushed into the worklist for n times,
// then there is a negative cycle.
// TODO: use priority_queue to increase performance!
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
// NB: it also help calculate src to all-point shortest path.
// The one-sink is just to initialize differently.
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

        // Instead of this, we can iterate all the edges directly.
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
                // NO! since the intermediate node is k.
                res[i][j] = min(res[i][k] + res[k][j], res[i][j]);
            }
        }
    }
}

// in a DAG, directly acyclic graph. Such a graph has a topological order.
// dfs, but push to stack *after* its children
void help_get_topo(int v, int **g, int n, stack<int> &s, set<int> &visited)
{
}

vector<int> get_topo(int **g, int n)
{
    vector<int> ret;
    {
        set<int> visited;
        stack<int> s;
        for(int i = 0; i < n; ++i) {
            help_get_topo(v, g, n, s, visited);
        } 
        while(!s.empty()) {
            ret.push_back(s.peek());
            s.pop();
        }
    }
    return ret;
}

class graph_t
{
    struct edge_t {
        int idx, weight;

        edge_t(int idx, int weight)
          : idx(idx),
            weight(weight) {}
    };

    struct node_t{
        int idx;
        vector<edge_t> adjs;

        node_t(int idx): idx(idx) {}

        void add_adj(int d, int w) {
            adjs.push_back(edge_t(d, w));
        }
    };

    map<int, node_t> nodes;

    void add_edge(int s, int d, int w) {
        if(!contains(nodes, s)) {
            nodes.insert(pair<int, node_t>(s, node_t(s)));
        }
        nodes[s].add_adj(d, w);
    }

    vector<int> get_adjs(int s) {
        return vector<int>();
    }

    int dist(int s, int d) {
        return nodes[s][d];
    }

    graph_t reversed_graph() {
        graph_t ret;
        foreach(i, nodes) {
            node_t &n = i->second;
            foreach(j, n->adjs) {
                edge_t &e = *j;
                ret.add_adj(e.idx, s, e.weight);
            }
        }
        return ret;
    }

    int help_compute_scc(int v, 
                         int &idx
                         stack<int> &s, 
                         stack<stack<int> > &dag,
                         map<int, int> &sccs,
                         map<int, int> &idxes) {
        if(contains(sccs, v)) {
            return sccs[v]; 
        }

        sccs[v] = idxes[v] = idx++;
        s.push(v);
        foreach(it, get_adjs(v)) {
            sccs[v] = min(sccs[v], help_compute_scc(*it, idx, s, dag, sccs, idxes));
        }

        if(sccs[v] == idxes[v]) {
            stack<int> sub;
            while(!s.empty() && s.peek() != v){
                sub.push(s.peek());
                s.pop();
            }
            dag.push(sub);
        }
    }

    // compute dag of sccs
    void compute_scc() {

        int idx = 0;
        stack<int> scc_stack;
        stack<stack<int> > dag;
        map<int, int> sccs, idxes;
        foreach(i, nodes) {
            (void)help_compute_scc(i->first, idx, scc_stack, dag, sccs, idxes);
        }

        while(!dag.empty()) {
            //
        }
    }
    
    vector<int> topo;
    bool topoed;
    vector<int> topo_sort() {
        if(!topoed) {
            compute_sccs();
        }
        return topo;
    }

    // compute biconnected component, which is to find the articulation point.
    // compared to propagating sccs, always taking the min(sccs) to get the
    // biggest circle;
    // it propagates a lowpt in tree edge, when back-edge, it takes min(num,
    // lowpt).
    int help_compute_bcc(int v, 
                         int &idx,
                         stack<int> s,
                         map<int,int> bcces,
                         map<int,int> idxes)
    {
        lowpts[v] = bccs[v] = idx++;
        s.push(v);

        foreach(n, get_adjs(v)) {
            // tree edge
            if(!contains(idxes, n)) {
                lowpts[v] = min(lowpts[v], help_compute_bcc(v, idx, s, bccs, idxes));
            } else {
                // back edge
                lowpts[v] = min(lowpts[v], idxes[n]);
            }
        }

        if(lowpts[v] == bccs[v]) {
            // pop out the biconnected component
        }
    }

    void compute_bcc() {
        int idx = 0;
        stack<int> bcc_stack;
        map<int, int> bccs, idxes;
        foreach(i, nodes) {
            (void)help_compute_bcc(i->first, idx, bcc_stack, bccs, idxes);
        }
    }
};

// bi-connected
