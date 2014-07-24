#include "container.hpp"
#include "bitset.hpp"

typedef vector<string>::iterator vstr_it_t;
typedef bitset_t<26> charset_t;

charset_t common_chars(vstr_it_t begin, vstr_it_t end)
{
    if(begin + 1 == end)
    {
        charset_t set;
        foreach(it, *begin) {
            set.set((*it - 'a'));
        }
        return set;
    }

    vstr_it_t mid =  begin + (end - begin)/2;
    charset_t left = common_chars(begin, mid);
    charset_t right = common_chars(mid, begin);

    return left & right;
}
int common_chars(vector<string> inputs)
{
    if(inputs.size() < 2) {
        return 0;
    } 
    return common_chars(inputs.begin(), inputs.end()).size();
}
