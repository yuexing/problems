// a class encapsulating a bitmap using 8 bits, an unsigned char.
class BitMapByte
{
private:
    enum { NUM_BITS = 8 };

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
