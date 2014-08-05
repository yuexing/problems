// Hamming weight (number of set bits)
template<typename T>
int hammingWeight(T u)
{
    int size = sizeof(T);
    int log = 1;
    while((1 << log) < size) log++;
    assert((1 << log) == size); // 8(3), 16(4), 32(5)

    for(int i = 0; i < log; ++i) {
        int unit = (1 << i);
        u |= (u >> unit);
    }
    return u;
}

// swap(reverse) bits in a byte
uint8_t swap1(uint8_t c)
{
    uint8_t ret = 0;
    uint8_t idx = 7;

    while(c) {
        ret |= (c & 1) << (idx--);
        c >>= 1;
    }
    return ret;
}

// find if byte a palindrome or not
bool is_palindrome(uint8_t c)
{
    uint8_t i = 0, j = 7;
    while(i < j) {
        if((c & (1 << i)) != (c & (1 << j))) {
            return false;
        }
    }
    return true;
}

// Template to find the best integral type to store a number of
// bits, using template specialization.
template<int bits> struct SizedTypeForBits {
};

#define DEF_INT_WITH_THIS_MANY_BITS(nbits, type)    \
    template<> struct SizedTypeForBits<nbits> {     \
        typedef type theType;                       \
    }

// 8 bits or less
// Using octal for nbits since we're dealing with multiples of 8
DEF_INT_WITH_THIS_MANY_BITS(00, uint8);
DEF_INT_WITH_THIS_MANY_BITS(01, uint8);
DEF_INT_WITH_THIS_MANY_BITS(02, uint8);
DEF_INT_WITH_THIS_MANY_BITS(03, uint8);
DEF_INT_WITH_THIS_MANY_BITS(04, uint8);
DEF_INT_WITH_THIS_MANY_BITS(05, uint8);
DEF_INT_WITH_THIS_MANY_BITS(06, uint8);
DEF_INT_WITH_THIS_MANY_BITS(07, uint8);
DEF_INT_WITH_THIS_MANY_BITS(010, uint8);

// 16 bits or less
DEF_INT_WITH_THIS_MANY_BITS(011, uint16);
DEF_INT_WITH_THIS_MANY_BITS(012, uint16);
DEF_INT_WITH_THIS_MANY_BITS(013, uint16);
DEF_INT_WITH_THIS_MANY_BITS(014, uint16);
DEF_INT_WITH_THIS_MANY_BITS(015, uint16);
DEF_INT_WITH_THIS_MANY_BITS(016, uint16);
DEF_INT_WITH_THIS_MANY_BITS(017, uint16);
DEF_INT_WITH_THIS_MANY_BITS(020, uint16);

// 32 bits or less
DEF_INT_WITH_THIS_MANY_BITS(021, uint32);
DEF_INT_WITH_THIS_MANY_BITS(022, uint32);
DEF_INT_WITH_THIS_MANY_BITS(023, uint32);
DEF_INT_WITH_THIS_MANY_BITS(024, uint32);
DEF_INT_WITH_THIS_MANY_BITS(025, uint32);
DEF_INT_WITH_THIS_MANY_BITS(026, uint32);
DEF_INT_WITH_THIS_MANY_BITS(027, uint32);
DEF_INT_WITH_THIS_MANY_BITS(030, uint32);
DEF_INT_WITH_THIS_MANY_BITS(031, uint32);
DEF_INT_WITH_THIS_MANY_BITS(032, uint32);
DEF_INT_WITH_THIS_MANY_BITS(033, uint32);
DEF_INT_WITH_THIS_MANY_BITS(034, uint32);
DEF_INT_WITH_THIS_MANY_BITS(035, uint32);
DEF_INT_WITH_THIS_MANY_BITS(036, uint32);
DEF_INT_WITH_THIS_MANY_BITS(037, uint32);
DEF_INT_WITH_THIS_MANY_BITS(040, uint32);

// 64 bits or less
DEF_INT_WITH_THIS_MANY_BITS(041, uint64);
DEF_INT_WITH_THIS_MANY_BITS(042, uint64);
DEF_INT_WITH_THIS_MANY_BITS(043, uint64);
DEF_INT_WITH_THIS_MANY_BITS(044, uint64);
DEF_INT_WITH_THIS_MANY_BITS(045, uint64);
DEF_INT_WITH_THIS_MANY_BITS(046, uint64);
DEF_INT_WITH_THIS_MANY_BITS(047, uint64);
DEF_INT_WITH_THIS_MANY_BITS(050, uint64);
DEF_INT_WITH_THIS_MANY_BITS(051, uint64);
DEF_INT_WITH_THIS_MANY_BITS(052, uint64);
DEF_INT_WITH_THIS_MANY_BITS(053, uint64);
DEF_INT_WITH_THIS_MANY_BITS(054, uint64);
DEF_INT_WITH_THIS_MANY_BITS(055, uint64);
DEF_INT_WITH_THIS_MANY_BITS(056, uint64);
DEF_INT_WITH_THIS_MANY_BITS(057, uint64);
DEF_INT_WITH_THIS_MANY_BITS(060, uint64);
DEF_INT_WITH_THIS_MANY_BITS(061, uint64);
DEF_INT_WITH_THIS_MANY_BITS(062, uint64);
DEF_INT_WITH_THIS_MANY_BITS(063, uint64);
DEF_INT_WITH_THIS_MANY_BITS(064, uint64);
DEF_INT_WITH_THIS_MANY_BITS(065, uint64);
DEF_INT_WITH_THIS_MANY_BITS(066, uint64);
DEF_INT_WITH_THIS_MANY_BITS(067, uint64);
DEF_INT_WITH_THIS_MANY_BITS(070, uint64);
DEF_INT_WITH_THIS_MANY_BITS(071, uint64);
DEF_INT_WITH_THIS_MANY_BITS(072, uint64);
DEF_INT_WITH_THIS_MANY_BITS(073, uint64);
DEF_INT_WITH_THIS_MANY_BITS(074, uint64);
DEF_INT_WITH_THIS_MANY_BITS(075, uint64);
DEF_INT_WITH_THIS_MANY_BITS(076, uint64);
DEF_INT_WITH_THIS_MANY_BITS(077, uint64);
DEF_INT_WITH_THIS_MANY_BITS(0100, uint64);

#undef DEF_INT_WITH_THIS_MANY_BITS

