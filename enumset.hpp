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
};
