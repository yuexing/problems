#ifndef _CONTAINER_HPP
#define _CONTAINER_HPP

#include <vector>
#include <list>
#include <map>
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

// what is desired here?
struct array_wrapper
{
};

#define NO_OBJ_COPIES(type)     \
  private:                      \
    type(const type&);          \
    type &operator&(const type&);\

template<typename C>
struct printable_container_t
{
    printable_container_t(const C& c, char sep=',')
      : c(&c) {}

    void print(ostream &os) {
    }
private:
    const C *c;
    char sep;
};

template<typename C>
printable_container_t<C> printable_container(C c)
{
    return printable_container_t(c);
}

ostream &operator<<(ostream os, const printable_container_t &p)
{
    p.print(os);
}

#endif
