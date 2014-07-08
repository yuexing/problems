#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <iostream>
#include <cstdlib>
using namespace std;

inline void xabort(const string str, const char* file, int line)
{
    cerr << file << ": " << line << ": " << str << endl;
    abort();
}

#define xfailure(str) \
    xabort(str, __FILE__, __LINE__)

#define string_assert(cond, str) \
    (void)((cond) ? 1 : (xfailure(str), 0))

#define cond_assert(cond) \
    string_assert(cond, #cond)

#define DOUT(flag, str)                 \
    do{                                 \
        if(debug::flag) {               \
            cout << __func__ << ": "    \
                << __LINE__ << ": "     \
                << str << endl;         \
        }                               \
    } while(0)

#endif /* DEBUG_HPP */
