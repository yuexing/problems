#ifndef VARWIDTH_H
#define VARWIDTH_H

// Maximum number of bytes to encode a number
#define VARWIDTH_MAX_WIDTH 9

// The format:
//   0xxx xxxx     1 byte
//   10xx xxxx     2 bytes
//   110x xxxx     3 bytes
//   1110 xxxx     4 bytes
//   1111 0xxx     5 bytes
//   1111 10xx     6 bytes
//   1111 110x     7 bytes
//   1111 1110     8 bytes
//   1111 1111     9 bytes
// 'x' bits are the encoded value and they are also in network byte order (big
// endian). The quantity is two's complement signed.


// Encode 'value' by writing up to VARWIDTH_MAX_WIDTH bytes into '**dest', moving
// '*dest' to point to the byte just after the last one written.
// '**dest' must have enough space to allow this.
void varWidthEncodeInt64(unsigned char **dest, sint64 value)
{
    unsigned char* buf = *dest;
    for(int nbytes = 1; nbytes <= 8; ++nbytes) {
        sint64 lo = -(1 << (nbytes * 8 - nbytes)),
               hi = -(lo + 1);
        if(value <= hi && value >= lo) {
           unsigned char val = value >> ((nbytes - 1) * 8);
           val |= 0xfe << (8 - nbytes);
           buf[0] = val;

           for(int j = 1; j < nbytes; ++j) {
                buf[j] = (value >> (nbytes -j - 1) * 8);
           }

           *dest = buf + nbytes;
        }
    }
    // 9
    buf[0] = 0xff;
    for(int j = 1; j < 9; ++j) {
        buf[j] = (value >> (8 - j) * 8);
    }
    *dest = buf + 9;
}

// Decode the next value from '**src', storing the decoded value in
// '*value' and advancing '*src' to just past the last read byte.
//
// If there is an error, its code is returned, and neither '*src' nor
// '*value' are modified.
void varWidthDecodeInt64(unsigned char const **src, sint64 *value)
{
    unsigned char* buf = *src;

    int nbytes = 1;
    unsigned char base = buf[0];
    while(base & 0x80) {
        ++nbytes;
        base <<= 1;
    } 

    *value = 0;
    if(nbytes == 9) {
        for(int j = 1; j < 9; ++j) {
            *value |= buf[j] << (8 - j);
        }
    } else {
        // first bytes
        *value |= (buf[0] & (~(0xfe << (8 - nbytes)))) << (nbytes - 1);
        // others
        for(int j = 1; j < nbytes; ++j) {
            *value |= buf[j] << (nbytes - j - 1);
        }
    }

    *src += nbytes;
}

#endif /* VARWIDTH_H */
