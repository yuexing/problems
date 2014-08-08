#include "container.hpp"
// recursive intuition:
// [i, j] = 1 + [i-1, j-1];         if p[i] == q[j]
//          max([i-1, j], [i, j-1]);otherwise
// bottom-up:
// 1) arr[2][q.size()]
int LCS(string p, string q)
{
    return 0;
}

// [i, j] = 1 + [i-1, j-1]; if p[i] == q[j]
//        = 0;              otherwise
// or:
// merge the array of suffixes. 
// or:
// LCA of the suffix-tree.
int LCStr(string p, string q)
{
    return 0;
}

// [i, j] = 0 + [i-1, j-1];     equal
//        = 1 + [i-1, j];       delete
//        = 1 + [i, j-1];       add
// take the max.
int edit_dist(string p, string q)
{
    return 0;
}

// KMP
// partial table (failure function) : T: records the length of the matched real prefix 
// q: ABCDABD
// once when there is a mismatch, decides start from which part of q.
bool strstr(string p, string q)
{
    // calculate partial table T
    int *T = new int[q.size()];
    T[0]= 0;
    for(int i = 1, j = 0; i < q.size(); ++i) {
        if(q[i] == q[j]) {
            T[i] = ++j;
        } else {
            do {
                j = T[j];
            } while(j != 0 && q[i] != q[j]);

            if(q[i] == q[j]) {
                T[i] = ++j;
            } else {
                T[i] = 0;
            }
        }
    }

    // strstr
    for(int i = 0, j = 0; i < p.size(); ++i)
    {
        if(p[i] == q[j]) {
            if(j == q.size() - 1) {
                return true;
            } else {
                ++j;
            }
        } else {
            do {
                j = T[j];
            } while(j != 0 && q[i] != q[j]);
        }
    }

    return false;
}

// text justification routine
// Greedy solution:
// counter case:
// [aaa bb cc ddddd] 6
// Dynamic solution:
// 1) line-cost: lc[i, j] the cost for puting words [i, j) in one line
// 2) cost: c[i] = c[j-1] + lc[j, i]

// Given two (dictionary) words as Strings, determine if they are isomorphic. Two words are called isomorphic 
// if the letters in one word can be remapped to get the second word. 

// given a larger file(something like that), report the top k most frequent
// words.
// The intuition is to count and minHeap
// to combine them, need a trie and a minHeap

// implementing with hashmap and minHeap can also solve the problem, with 
// O(n + nlg(k))
#define SIZE 26

struct trieAndHeap
{
    struct node_t
    {
        bool is_end;
        int count;  
        int heapIdx;

        node_t *children[SIZE];

        node_t()
            : is_end(false),
              count(0),
              heapIdx(-1) {
                  memset(children, SIZE * sizeof(node_t*), 0);
              }
    };

    static int cmp_node_t(const node_t& n1, const node_t& n2)
    {
        return 0;
    }

    struct minHeap
    {
        minHeap(int capacity)
            : capacity(capacity), 
              size(0),
              arr(new node_t*[capacity]) {
        }

        void addOrUpdate(node_t *n)
        {
            if(n->heapIdx != -1) {
                filterDown(n);
            }

            if(size == capacity) {
                if(n->count > arr[0]->count) {
                    arr[0]->heapIdx = -1;
                    arr[0] = n;
                    n->heapIdx = 0;
                    filterDown(n);
                }
            } else {
                arr[size] = n;
                n->heapIdx = size++;
                filterUp(n);
            }
        }
    private:
        int capacity;
        int size;
        node_t **arr;

        void filterDown(node_t *n)
        {
        }

        void filterUp(node_t *n)
        {
        }
    };

    int insert(const string &s)
    {
        return insert(root, s.c_str());
    }

    void print()
    {
    }
private:
    node_t *root;
    minHeap heap;

    int insert(node_t *root, const char* s)
    {
        node_t *n = root->children[s[0]];
        if(!n) {
            n = root->children[s[0]] = new node_t;
        }
        if(s[1] == '\0') {
            n->is_end = true;
            n->count++;
            
            // also put into the minHeap
            heap.addOrUpdate(n);
        } else {
            insert(n, s + 1);
        }
    }
};

// Write code to implement huffman coding of a string

// dist btw words in a text.
static vector<string> words;
// NB: the \p list1,\p  list2 stores the positions
// [1,4,7,9]
// [3,6,11,15]
int find_minDist(vector<int> list1, vector<int> list2)
{
    int min = words.size();
    int i = 0, j = 0;
    for(; i < list1.size() && j < list2.size(); )
    {
        int dist = abs(list1[i] - list2[j]);
        if(list1[i] < list2[j]) {
            ++i;
        } else {
            ++j;
        }
        if(dist < min) {
            min = dist;
        }
    }
    // take care about the last one.
    if(i == list1.size()) {

    } else if (j == list2.size()) {

    }
    return min;
}

// what if iterating just on the text
// no duplicates is allowed here.
int find_minDist2(vector<string> text, string word1, string word2)
{
    int loc1 = -1, loc2 = -1, min = text.size();
    for(int i = 0; i < text.size(); ++i) {
        if(text[i] == word1) {
            loc1 = i;
        } else if(text[i] == word2) {
            loc2 = i;
        } else continue;

        if(loc1 != -1 && loc2 != -1) {
            int dist = abs(loc1 - loc2);
            if( dist < min) {
                min = dist; 
            }
        }
    }
    return min;
}

// what if we have bunch of words to look for
// and also allow duplication for the tofind
// NB: also referred to as minWindow.
int find_minDist3(vector<string> text, vector<string> tofind)
{
    map<string, int> needs;
    foreach(it, tofind) {
        ++needs[*it];
    }

    map<string, int> founds;
    queue<int> found_locs;
    int count = 0, begin = 0, min = text.size();
    for(int i = 0; i < text.size(); ++i) {
        if(!contains(needs, text[i])) {
            continue;
        }
        ++founds[text[i]];
        found_locs.push(i);
        if(founds[text[i]] < needs[text[i]]) {
            ++count;
        }
        if(count == tofind.size()) {
            // pop out if possible
            do {
                int idx = found_locs.front();
                if(founds[text[idx]] > needs[text[i]]) {
                    begin = idx;
                    found_locs.pop();
                } else {
                    break;
                }
            } while(1);
            // now update the min
            int dist = i - begin;
            if(dist < min) {
                min = dist;
            }
        }
    }
    return min;
}

// implement path simplification
// implement cd
void split(string s, char sep, list<string> v)
{
    const char *cs = s.c_str();
    for(int i = 0, begin = 0; i < s.size(); ++i) {
        if(cs[i] == sep) {
            if(i - begin > 1) {
                v.push_back(s.substr(begin, i - begin));
            }
            begin = i;
        } 
    }
}

string join(list<string> v, char sep)
{
    return "";
}

string cd(string path)
{
    string pwd = getenv("PWD");
    list<string> parts;
    split(pwd, '/', parts);
    split(path, '/', parts);
    LET(i, parts.begin());
    LET(j, parts.begin());
    while(i != parts.end()) {
        if(*i == ".") {
            i = parts.erase(i);
        } else if(*i == "..") {
            parts.erase(i - 1);
            i = parts.erase(i);
        } else {
            *j = *(i++);
        }
    }
}
extern bool str_equal(const string &s1, const string &s2);
// for c-string
extern bool str_equal(const char* s1, const char *s2);

#include <fstream>
// parse a log, print the concurrent processes at a moment.
void print_concurrency(const char* log)
{
    ifstream ifs(log, ifstream::in);
    set<int> curs;
    while(ifs) {
        string line;
        getline(ifs, line);
        line.clear();

        list<string> parts;
        split(line, ',', parts);

        int pid = atoi(parts[0]);
        int time = atoi(parts[1]);
        if(str_equal(parts[2], "start")) {
            curs.insert(pid);
        } else if(str_equal(parts[2], "end")) {
            curs.erase(pid);
        }
        /*
        cout << "at time " << time << ": "
          << printable_container(curs); */
    }
}

// print all palindromes of size k possible from given alphabet set.
// eg alphabet set : {a,e,i,o,u}
// print all palindromes of size say 10.
extern gen_help(int i, int j, int k, string &buf, const string &cset);
void gen(int k, const string cset)
{
    string buf(cset.size());
    int i = -1, j = -1;
    if(k % 2 == 1) {
        i = k/2;
        j = k/2 + 2;
        foreach(it, cset) {
            buf[k/2 + 1] = *it;
            gen_help(i, j, k, buf, cset);
        }
    } else {
        i = k/2;
        j = k/2 + 1;
        gen_help(i, j, k, buf, cset);
    }

}

// regex:
// metachar: . * ? ^ $
bool matches(const char* regex, const char *s, int i, int j /*, mem*/)
{
    if(regex[i] == '.') {
        return matches(regex, s, i + 1, j);
    } else if(regex[i] == '*') {
        int k = j;
        while(s[k] == s[k-1]) {
            if(matches(regex, s, i + 1, k)) {
                return true;
            }
        }
    } else if(regex[i] == '?') {
        if(matches(regex, s, i + 1, j)) {
            return true;
        }
        return s[j] == s[j-1] && matches(regex, s, i + 1, j + 1);
    } else if(regex[i] == '$') {
        return s[j] == '\0';
    } else return regex[i] == s[j];
}

bool match(const char* regex, const char* s)
{
if(regex[0] == '^') {
    return matches(regex + 1, s);
} else {
    for(int i = 0; i < strlen(s); ++i) {
        if(matches(regex, s + i)) {
            return true;
        }
    }
}
}

// hufman
void hufman(const string s)
{
    // 
    map<char, int> counts;
    // 
    foreach(it, counts) {
        //minQueue.add(new node(it->first, it->second);
    }
    /* 
    while(!minQueue.size() > 1) {
        minQueue.pop();
        minQueue.pop();
        new node('\0', 1 + 2);
        minQueue.add();
    }*/
}

// generate BST given preorder

// longest prefix, suffix
// nlogn?
