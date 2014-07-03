#include <iostream>
using namespace std;

#include "dict.hpp"

// TODO: print out usage
void usage()
{
    cerr << "Error" << endl;
    return;
}

int main(int argc, const char* argv[])
{
    if(argc < 2) {
        usage();
        return -1;
    }
    dict_t dict;
    dict.init(argv[1]);
    return 0;
}
