// a wrapper for a set of enums. Given a enum with values 0,1,2,... each will
// be represented as a bit in an IntType. The trickest part is to decide the
// IntType and it has been achieved by the bits/bits.hpp

// The supported operations:
// add/substract/intersect/union/all/none/iteration
template<typename EnumType, int nValues> 
class EnumSet
{
private:
    typedef typename SizeTypeForBits<nValues>::theType IntType;
    IntType data;

    EnumSet(): data(0) {}
    EnumSet(int data): data(data) {}
    
    struct iterator_t {
        iterator_t(int val): val(val) {
            next_one();
        }

        void operator++() {
            next_one();
        }
        int operator*() {
            return n;
        }

        bool operator==(const iterator_t &o) {
            return val == o.val;
        }

    private:
        void next_one() {
            int oldval = val;
            val = (val - 1) & val;
            int diff = oldval - val;
            int m = 0;
            while(diff > 1) {
                oneval >>= 1;
                ++m;
            }
            val >>= m;
            n += m;
        }
        IntType val;
        int n;
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

    iterator_t begin() {
        return iterator_t(data);
    }

    iterator_t end() {
        return iterator_t(0);
    }

};
