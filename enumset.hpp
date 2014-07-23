// a wrapper for a set of enums. Given a enum with values 0,1,2,... each will
// be represented as a bit in an IntType. The trickest part is to decide the
// IntType and it has been achieved by the bits/bits.hpp

// The supported operations:
// add/substract/intersect/union/all/none/iteration

// NB: how nValues is used.
template<typename EnumType, int nValues> 
class EnumSet
{
private:
    typedef typename SizeTypeForBits<nValues>::theType IntType;
    IntType data;

    EnumSet(): data(0) {}
    EnumSet(int data): data(data) {}
    
    struct iterator_t {
        iterator_t(EnumSet& set, int bitidx)
            : set(set),
              bitidx(bitidx) {
            findNext();
        }

        iterator_t& operator++() {
            bitidx++;
            findNext();
            return *this;
        }
        EnumType operator*() {
            return (EnumType)bitidx;
        }

        bool operator==(const iterator_t &o) {
            return &set == &o.set && bitidx == &o.bitidx;
        }

    private:
        void findNext() {
            while(bitidx < nValues && !set.contains((EnumType)bitidx)) {
                ++bitidx;
            }
        }

        EnumSet &set;
        int bitidx;
    };

public:
    EnumSet single(int val) {
        assert(val < nValues);
        return EnumSet(1 << val);
    }

    EnumSet none() {
        return EnumSet();
    }

    EnumSet all() {
        return EnumSet(1 << sizeof(data) >> (sizeof(data) - nValues));
    }

    EnumSet operator|(const EnumSet o) {
        return EnumSet(data | o.data); 
    }

    bool contains(EnumType i) 
    {
        cond_assert(i < nValues);
        return (data && 1 << i);
    }

    iterator_t begin() {
        return iterator_t(*this, 0);
    }

    iterator_t end() {
        return iterator_t(*this, nValues);
    }

};
