#ifndef _CONTAINER_UTIL_HPP
#define _CONTAINER_UTIL_HPP

#include <map>
#include <vector>
#include <iostream>

/// avoid long type declaration
#define LET(a, b) \
  typeof(b) a(b)

/// foreach util
#define foreach(it, c) \
  for(LET(it, (c).begin()); it != (c).end(); ++it)

#define reverse_foreach(it, c) \
  for(LET(it, (c).end()); it-- != (c).begin(); ++it)

/// general
template<typename C, typename K>
inline bool contains(C c, K k)
{
  return c.find(k) != c.end();
}

/// print containers
/// usage: ostream << printable_container(c);
template<typename C>
struct printable_container_t
{
  const C& c;
  char seperator;

  printable_container_t(const C& c, char seperator)
    : c(c), seperator(seperator)
  {}

  virtual void print(std::ostream &os) const {
    foreach(it, c) {
      os << *it << seperator;
    }
  }
};

template<typename K, typename V>
struct printable_container_t<std::map<K,V> > {

  const std::map<K,V> c;
  char seperator;

  printable_container_t(const std::map<K, V>& c, char seperator)
    : c(c), seperator(seperator)
  {}

  void print(std::ostream &os) const
  {
    foreach(it, c) {
      os << "("<< it->first << "," << it->second << ")" << seperator;
    }
  }
};


template<typename C>
inline std::ostream& operator<<(std::ostream &os, 
    const printable_container_t<C> &pc)
{
  pc.print(os);
  return os;
}

template<typename C>
inline printable_container_t<C> printable_container(const C& c,
    char seperator = ',')
{
  return printable_container_t<C>(c, seperator);
}

#endif
