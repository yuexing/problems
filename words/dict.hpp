#ifndef DICT_HPP
#define DICT_HPP

#include <string>
#include <vector>
#include <map>
using namespace std;

namespace debug {
    extern bool dict;
};

/*
 * dict_t: store the words in a sorted vector(words), so that check-existence 
 * can be done in log(n).
 * Also, store the index to the words by the length of the words in lengths.
 * NB: the words in the file are supposed to be sorted, thus throw exception
 * if it is not.
 */
struct dict_t
{
public:

    void init(const char *path);
    bool is_composition(const string& s);
    string find_longest();
    void find_all(vector<string>&);

private:
    void add(const string& word);
    bool check_sorted();
    void fill_indexes();
    bool contains(const string& s);
    bool is_composition(const string& s, bool inclusive, int min, int max);

    vector<string> words;
    map<int, int> lengths;
    vector<int> indexes;
};

#endif /* DICT_HPP */
