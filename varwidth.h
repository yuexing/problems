#ifndef VARWIDTH_H
#define VARWIDTH_H
enum VarWidthError {

  VWE_NO_ERROR = 0,          // no error; success
  VWE_ENCODING_TOO_LARGE,    // the encoded bytes decode to a value too large for this machine
};
typedef enum VarWidthError VarWidthError;

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

}
// 32-bit version. Could be made more efficient on 32-bit platforms
// but we currently don't do it. Doesn't write more than 5 bytes.
void varWidthEncodeInt32(unsigned char **dest, sint32 value)
{

}

// Decode the next value from '**src', storing the decoded value in
// '*value' and advancing '*src' to just past the last read byte.
//
// If there is an error, its code is returned, and neither '*src' nor
// '*value' are modified.
void varWidthDecodeInt64(unsigned char const **src, sint64 *value);
// This version can return VWE_ENCODING_TOO_LARGE
VarWidthError varWidthDecodeInt32(unsigned char const **src, sint32 *value);

// Return the total number of bytes used in the encoding that begins
// with 'firstByte', with that byte included in the count.  Return
// value is between 1 and 9, inclusive, unless the first byte is not a
// valid initial byte, in which case 0 is returned.  This function
// does not take into account whether the encoded value can be
// represented as a 'long' on the host machine.
//
// The intended purpose is to allow a deserializer to read one byte,
// then use this function to determine how many more bytes need to be
// read (which is this function's return value minus one).
int getVarWidthEncodingSizeFromFirstByte(unsigned char firstByte);

// Number of bytes required to encode this value.
int getVarWidthEncodingSizeForInt64(sint64 value);


#endif /* VARWIDTH_H */
