// a class encapsulating a bitmap using 8 bits, an unsigned char.
#include "debug.hpp"
#define CHAR_SIZE sizeof(char)

class BitMapByte
{
public:
    BitMapByte(): data(0) {
    }
    BitMapByte(unsigned char data): data(data) {
    }
    BitMapByte(const BitMapByte& b): data(b.data) {
    }

    BitMapByte& operator=(const BitMapByte& b) {
        data = b.data;
        return *this;
    }
    bool operator[](int i) {
        return get(i);
    }
    BitMapByte operator+(const BitMapByte& b) const {
        return BitMapByte(data | b.data);
    }
    BitMapByte operator-(const BitMapByte& b) const {
        return BitMapByte(data & (~b.data));
    }
    BitMapByte operator&(const BitMapByte &b) const {
        return BitMapByte(data & b.data);
    }
    BitMapByte operator|(const BitMapByte &b) const {
        return BitMapByte(data | b.data);
    }

    BitMapByte& operator+=(const BitMapByte& b) {
        data |= b.data;
        return *this;
    }
    BitMapByte& operator-=(const BitMapByte& b) {
        data &= (~b.data);
        return *this;
    }
    BitMapByte& operator&=(const BitMapByte &b) {
        data &= b.data;
        return *this;
    }
    BitMapByte& operator|=(const BitMapByte &b) {
        data |= b.data;
        return *this;
    }

private:
    unsigned char data;

    bool get(int b) {
        return data & (1 << b);
    }
};

static int multipleof(int n, int unit)
{
    return (n + (unit - 1)) / unit;
}

template<int nbits>
struct bitset_t
{
    bitset_t()
      : nbytes(multipleof(nbits, CHAR_SIZE)),
        bytes(new char[nbytes]) {
    }

    bitset_t(char *bytes)
      : nbytes(multipleof(nbits, CHAR_SIZE)),
        bytes(bytes) {
        }

    bitset_t(const bitset_t &o)
      : nbytes(o.nbytes),
        bytes(new char[nbytes]) {
            for(int i = 0; i < nbytes; ++i) {
                bytes[i] = o.bytes[i];
            }
        }

    ~bitset_t() {
        delete[] bytes;
    }

    void set(int nbit)
    {
        cond_assert(nbit < nbits);
        set(idx(nbit), offset(nbit));
    }

    void unset(int nbit)
    {
        cond_assert(nbit < nbits);
        unset(idx(nbit), offset(nbit));
    }

    bool isset(int nbit)
    {
        cond_assert(nbit < nbits);
        return isset(idx(nbit), offset(nbit));
    }

    bitset_t operator|(const bitset_t<nbits>& o)
    {
        char *newbytes = new char[nbytes];
        for(int i = 0; i < nbytes; ++i) {
            newbytes[i] = bytes[i] | o.bytes[i];
        }
        return bitset_t<nbits>(newbytes);
    }

    bitset_t operator&(const bitset_t<nbits>& o)
    {
        char *newbytes = new char[nbytes];
        for(int i = 0; i < nbytes; ++i) {
            newbytes[i] = bytes[i] & o.bytes[i];
        }
        return bitset_t<nbits>(newbytes);
    }

    bitset_t operator-(const bitset_t<nbits>& o)
    {
        char *newbytes = new char[nbytes];
        for(int i = 0; i < nbytes; ++i) {
            newbytes[i] = bytes[i] & (~o.bytes[i]);
        }
        return bitset_t<nbits>(newbytes);
    }

    int size()
    {
        int n = 0;
        for(int i = 0; i < nbytes; ++i) {
            n + ones(bytes[i]);
        }
        return n;
    }

private:
    int nbytes;
    char* bytes;

    int idx(int nbit) {
        return nbit / CHAR_SIZE;
    }

    int offset(int nbit) {
        return nbit % CHAR_SIZE;
    }

    void set(int idx, int offset)
    {
        bytes[idx] |= (1 << offset);
    }

    void unset(int idx, int offset)
    {
        bytes[idx] &= (~(1 << offset));
    }

    bool isset(int idx, int offset)
    {
        return bytes[idx] & (1 << offset);
    }

    int ones(char i)
    {
        int n = 0;
        while(i) {
            ++n;
            i &= (i-1);
        }
        return n;
    }
};
