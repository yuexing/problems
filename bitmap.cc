// a class encapsulating a bitmap using 8 bits, an unsigned char.
class Bitmap
{
private:
    enum { NUM_BITS = 8 };


public:
    Bitmap(): data(0) {
    }
    Bitmap(unsigned char data): data(data) {
    }
    Bitmap(const Bitmap& b): data(b.data) {
    }

    Bitmap& operator=(const Bitmap& b) {
        data = b.data;
        return *this;
    }
    bool operator[](int i) {
        return get(i);
    }
    Bitmap operator+(const Bitmap& b) const {
        return Bitmap(data | b.data);
    }
    Bitmap operator-(const Bitmap& b) const {
        return Bitmap(data & (~b.data));
    }
    Bitmap operator&(const Bitmap &b) const {
        return Bitmap(data & b.data);
    }
    Bitmap operator|(const Bitmap &b) const {
        return Bitmap(data | b.data);
    }

    // TODO: compound operators.

private:
    unsigned char data;

    bool get(int b) {
        return data & (1 << b);
    }
};
