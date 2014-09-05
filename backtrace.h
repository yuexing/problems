#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <stdio.h>                              //fprintf
#include <execinfo.h>                           //backtrace
#include <stdlib.h>                             //free

/**
 * When trying to diagnose problem, it's helpful to know the path it goes
 * through.
 * Use NDEBUG to be consistent with assert().
 */

// through ret code
#ifdef NDEBUG
#define TRACKED_RET(n)      return (n);
#else
#define TRACKED_RET(n)                              \
    fprintf(stderr, "%s (%s, %d): returned %d\n",   \
            __FUNCTION__, __FILE__, __LINE__,       \
            n);                                     \
    return (n);
#endif

// through backtrace
static inline void write_bt()
{
    void *trace[20];
    size_t size = backtrace(trace, 20);
    char **symbols = backtrace_symbols(trace, size);

    if(!symbols) {
        fprintf(stderr, "stacktrace failed.");
        return;
    }

    fprintf(stderr, "stacktrace:\n");
    // use c++filt to demangle 
    // or use abi::__cxa_demangle demangle the output in the format
    // "module(function+0x15c) [0x8048a6d]". Note, ubuntu is not of such
    // format.
    for(i = 0; i < size; ++i) {
        fprintf(stderr, "%s\n", symbols[i]);
    }

    free(symbols);
}

#endif /* BACKTRACE_H */
