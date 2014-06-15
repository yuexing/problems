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
};
