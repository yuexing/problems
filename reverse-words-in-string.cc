/**
 * reverse the words in the string
 * delete leading / trailling spaces
 * reduce spaces btw words
 *
 * "   this is somthing "
 * => "something is this"
 *
 * NB: use string::resize to return the result.
 */
#include <string>
#include <iostream>
using namespace std;

class Solution {
public:
    void copy(string &s, int i, int j, int &t)
    {
        while(i <= j) {
            s[t++] = s[i++];
        }
    }

    void reverse(string &s, int i, int j)
    {
        while(i < j) {
            char tmp = s[i];
            s[i] = s[j];
            s[j] = tmp;
            i++; j--;
        }    


    }

    void reverseWords(string &s) {
        reverse(s, 0, s.size() - 1);

        int start = 0, end = 0, t = 0;
        while(s[start] && s[start] == ' ') start++;
        if(!s[start]) {
            s.resize(t);
            return;
        }

        end = start;
        while(1) {
            if(!s[end] || s[end] == ' ') {
                reverse(s, start, end - 1);
                copy(s, start, end - 1, t);
                if(!s[end]) {
                    s.resize(t);
                    return;
                }

                do{
                    ++end;

                    if(!s[end]) {
                        s.resize(t);
                        return;
                    }

                }while(s[end] == ' ');
                start = end;
                s[t++] = ' ';
            }
            ++end;
        }        
    } 
};

int main()
{
Solution *s =new Solution();
string str("  a b ");
s->reverseWords(str);
cout << "|" << str << "|"<< endl;
}
