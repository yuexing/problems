#ifndef _CONTAINER_UTIL_HPP
#define _CONTAINER_UTIL_HPP

#include <map>
#include <iostream>

/// avoid long type declaration
#define LET(a, b) \
  __typeof__(b) a(b)

#define LETREF(a, b) \
  __typeof__(b)& a(b)

#define LETCREF(a, b) \
  const __typeof__(b)& a(b)

/// const_iterator type
template<typename C>
class const_iterator_wrapper {
public:
  typedef typename C::const_iterator type;
};

template<typename C>
inline typename const_iterator_wrapper<C>::type get_const_iterator(const C& c)
{
  return const_iterator_wrapper<C>::type();
}

/// const_reverse_iterator
template<typename C>
class const_reverse_iterator_wrapper {
public:
  typedef typename C::const_reverse_iterator type;
};

template<typename C>
inline typename const_reverse_iterator_wrapper<C>::type get_const_reverse_iterator(const C& c)
{
  return const_reverse_iterator_wrapper<C>::type();
}

/// foreach util
#define foreach(it, c) \
  for(LET(it, (c).begin()); it != (c).end(); ++it)

#define cforeach(it, c) \
  for(__typeof__(get_const_iterator((c))) it = (c).begin(); it != (c).end(); ++it)

#define reverse_foreach(it, c) \
  for(LET(it, (c).rbegin()); it != (c).rend(); ++it)

#define creverse_foreach(it, c) \
  for(__typeof__(get_const_reverse_iterator((c))) it = (c).rbegin(); it != (c).rend(); ++it)

// erase all the elements that satisfy the \p predict.
// \p iter_ahead: used in condition to specify the current element
// \p seq: the sequential container
#define SEQ_ERASE(iter_ahead, seq, predicate)        \
  {                                                  \
    LET(iter_ahead, (seq).begin());                  \
    LET(iter_valid, (seq).begin());                  \
    for(; iter_ahead != (seq).end(); ++iter_ahead) { \
      if (!(predicate)) {                            \
        *(iter_valid++) = *iter_ahead;               \
      }                                              \
    }                                                \
    (seq).erase(iter_valid, (seq).end());            \
  }

/// general
template<typename C, typename K>
inline bool contains(C c, K k)
{
  return c.find(k) != c.end();
}

template<typename K, typename V>
inline std::ostream& operator<<(std::ostream &os, const std::pair<K,V> &p)
{
  os << "("<< p.first << "," << p.second << ")";
  return os;
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
    bool first = true;
    foreach(it, c) {
      if(first){
        first = false;
        os << *it;
      } else {
        os << seperator << *it;
      }
    }
  }
};

template<typename C>
inline std::ostream& operator<<(std::ostream &os, const printable_container_t<C> &pc)
{
  pc.print(os);
  return os;
}

template<typename C>
inline printable_container_t<C> printable_container(const C& c, char seperator = ',')
{
  return printable_container_t<C>(c, seperator);
}

#endif
