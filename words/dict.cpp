#include "dict.hpp"

#include <fstream>
using namespace std;

#include "stringb.hpp"
#include "debug.hpp"
#include "container-util.hpp"

namespace debug {
bool dict = true;
};

// add
void dict_t::add(const string &word)
{
    words.push_back(word);
    lengths[word.length()]++;
}

// check the vector words is sorted
bool dict_t::check_sorted()
{
    foreach(it, words)
    {
        if(it + 1 == words.end()) {
            return true;
        }
        if(*it > *(it + 1)) {
            return false;
        }
    }
    // hmm...uneachable unless empty
    return true;
}

// fill_indexes
void dict_t::fill_indexes()
{
    int total = 0;
    // get the starting index by accumulating
    foreach(it, lengths) {
        LET(j, it++);
        if(it == lengths.end()) {
            break;
        }
        total += j->second;
        it->second += it->second;
        it = j;
    }
    // allocate space
    DOUT(dict, "# of words: " << total);
    indexes.resize(total);
    // fill
    int idx = 0;
    foreach(it, words) {
        // no overwrite
        cond_assert(indexes[lengths[(*it).length()]] == 0);
        indexes[lengths[(*it).length()]++] = idx++;
    }
}

// init
void dict_t::init(const char *path)
{
    std::ifstream fin(path);

    string word;
    while(fin >> word) {
        add(word);
    }

    /*
    string_assert(check_sorted(), 
                  stringb("The words in file " << path << " to be sorted"));*/

    fill_indexes();
}
