#ifndef CONTAINER_UTIL_HPP
#define CONTAINER_UTIL_HPP

#define LET(a, b) \
    typeof(b) a(b)

#define foreach(it, c) \
    for(LET(it, (c).begin()); it != c.end(); ++it)

#define reverse_foreach(it, c) \
    for(LET(it, (c).end()); it-- != c.begin();)

#endif /* CONTAINER_UTIL_HPP */
