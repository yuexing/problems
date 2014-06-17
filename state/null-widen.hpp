// (c) 2007-2010 Coverity, Inc. All rights reserved worldwide.
#ifndef NULL_WIDEN_HPP
#define NULL_WIDEN_HPP

// Default state widener does no widening.
template<class T>
struct null_widen {
    bool operator()(state_t* st, const ASTNode *v, T& state, const T& other) {
        // Keep the value
        return true;
    }
};



#endif
