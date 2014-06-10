/**
 * Convert infix to post-fix:
 * operators:
 *      + -
 *      * /
 *      ()
 */
#include <map>
#include <vector>
#include <stack>
#include <iostream>
using namespace std;

#include "string.h"

class Solution {
public:
    Solution(): p()
    {
        p["+"] = p["-"] = 0;
        p["*"] = p["/"] = 1;
        p["("] = 2;
    }

    void toPost(const vector<string> &in)
    {
        stack<string> ops;

        for(vector<string>::const_iterator it = in.begin(); it != in.end(); ++it)
        {
            const string &s = *it;
            if( p.find(s) != p.end() ) {
                while( !ops.empty() && strcmp(ops.top().c_str(), "(") && p[ops.top()] >= p[s] ) {
                    cout << ops.top();
                    ops.pop();
                }
                ops.push(s);
            } else if( !strcmp(")", s.c_str()) ) {
                while( strcmp(ops.top().c_str(), "(") ) {
                    cout << ops.top();
                    ops.pop();
                }
                ops.pop(); // pop out the (
            } else {
                cout << s;
            }
        }
        while(!ops.empty()) {
            cout << ops.top();
            ops.pop();
        }
        cout << endl;
    }

private:
    map<string, int> p;
};

static vector<string> ret;
vector<string> get(const string& s)
{
    ret.clear();
    bool toadd = false;
    for(int i = 0, j = 0; s[i]; ++i) {
        if( s[i] ==  0x20 ) {
            if( toadd ) {
                toadd = false;
                ret.push_back(s.substr(j, i - j));
            }
        } else {
           if( !toadd ) {
            toadd = true;
            j = i;
           } 
        }
    }
    for(vector<string>::const_iterator it = ret.begin();
      it != ret.end();
      ++it) {
        cout << *it << ", ";
    }
    cout << endl;
    return ret;
}

int main()
{
    Solution *s = new Solution();
    s->toPost(get("1 + 2 + 3 + 4 "));
    s->toPost(get("1 + 2 * 3 + 4 "));
    s->toPost(get("1 * ( 2 + 3 ) * 4 "));
    return 0;
}
