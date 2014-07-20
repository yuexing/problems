#include "dict.hpp"

#include <fstream>
#include <algorithm>
using namespace std;

#include "stringb.hpp"
#include "debug.hpp"
#include "container-util.hpp"

namespace debug {
bool dict = false;
bool path = true; // TODO: debug the path for the is_composition (if necessary)
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
    foreach(it, words) {
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
    // get the starting index by accumulating the count
    foreach(it, lengths) {
        LET(j, it++);
        if(it == lengths.end()) {
            DOUT(dict, j->first << ": " << j->second);
            total = j->second;
            break;
        }
        it->second += j->second; //accumulate
        it = j;
        DOUT(dict, it->first << ": " << it->second);
    }
    // allocate space
    DOUT(dict, "# of words: " << total);
    indexes.resize(total);
    // fill
    int idx = words.size();
    reverse_foreach(it, words) {
        int len = (*it).length();
        // assert: count is correct
        cond_assert(lengths[len] > 0);
        // assert: no overwrite
        cond_assert(indexes[lengths[len] - 1] == 0);
        indexes[--lengths[len]] = --idx;
    }
}

// init: read words from the file indicated by the path to initilize the struct.
void dict_t::init(const char *path)
{
    std::ifstream fin(path);

    string word;
    while(fin >> word) {
        add(word);
    }

    string_assert(check_sorted(), 
                  stringb("The words in file " << path << " to be sorted"));

    fill_indexes();
}

// contains: is \p s one of the recorded words.
bool dict_t::contains(const string& s)
{
    return std::binary_search(words.begin(), words.end(), s);
}

// is_composition: is \p s made up of recorded words. 
bool dict_t::is_composition(const string& s)
{
    int min = words[indexes.front()].length();
    int max = words[indexes.back()].length();
    return is_composition(s, /*inclusive*/false, min, max + 1);
}

// is_composition: is \p s made up of recorded words. 
// \p inclusive indicates whether to include the word itself.
// \p min len(shortest-recorded-word) 
// \p max len(longest-recored-word) + 1
bool dict_t::is_composition(const string& s, bool inclusive, int min, int max)
{
    if(inclusive && contains(s)) {
        return true;
    }

    max = std::min(max, (int)s.length() - 1); 
    for(int i = min; i < max; ++i) {
        if(contains(s.substr(0, i))) {
            if(is_composition(s.substr(i), /*inclusive*/true, min, max)) {
                return true;
            }
        }
    }
    return false;
}

string dict_t::find_longest()
{
    reverse_foreach(it, indexes) {
        if(is_composition(words[*it])) {
            return words[*it];
        }   
    }
    return "";
}

void dict_t::find_all(vector<string>& v)
{
    reverse_foreach(it, indexes) {
        if(is_composition(words[*it])) {
            v.push_back(words[*it]);
        } 
    }
}
