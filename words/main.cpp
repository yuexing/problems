#include <iostream>
using namespace std;

#include "dict.hpp"
#include "container-util.hpp"

void usage(const char* exe)
{
    cerr << "Usage:" << exe << " file " << endl;
    return;
}

int main(int argc, const char* argv[])
{
    if(argc < 2) {
        usage(argv[0]);
        return -1;
    }
    dict_t dict;
    dict.init(argv[1]);
    cout << "longest: " << dict.find_longest() << endl;

    vector<string> v;
    dict.find_all(v);
    cout << "All(" << v.size() << "):" << endl;
    foreach(it, v) {
        cout << *it << endl;
    }
    return 0;
}
