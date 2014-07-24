#ifndef _CONTAINER_HPP
#define _CONTAINER_HPP

#include <vector>
#include <set>
#include <queue>
#include <stack>
#include <string>
#include <algorithm>
using namespace std;

#define LET(a, b) \
  typeof(b) a(b)

#define foreach(it, c) \
  for(LET(it, (c).begin()); it != (c).end(); ++it)

#define reverse_foreach(it, c) \
  for(LET(it, (c).end()); it-- != (c).begin(); ++it)

template<typename C, typename K>
inline bool contains(C c, K k)
{
    return c.find(k) != c.end();
}

struct array_wrapper
{
};

#endif
