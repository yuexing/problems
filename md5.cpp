/*
  Copyright (C) 1999, 2000, 2002 Aladdin Enterprises.  All rights reserved.

 Portions modified by Coverity, Inc.
 (c) 2004-2013 Coverity, Inc. All rights reserved worldwide.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */
/* $Id$ */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
        http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
        either statically or dynamically; added missing #include <string.h>
        in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
        type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
        unsigned in ANSI C, signed in traditional"; made test program
        self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */
#ifdef COVERITY_COMPILE_AS_C
#include "md5.h"
#else
#include "md5.hpp"
#include "file/xfile.hpp"              // file_does_not_exist_error
#include "libs/file/efstream.hpp"      // eifstream
#include "libs/file/file-operations.hpp"      // file_exists
#include "libs/containers/hash.hpp"      // file_exists
#include "string-utils.hpp" // hex_digit
#endif // COVERITY_COMPILE_AS_C
#include "macros.hpp"       // STATIC_ASSERT
#include <assert.h>
#include <string.h>         // memset

#undef BYTE_ORDER       /* 1 = big-endian, -1 = little-endian, 0 = unknown */
#ifdef ARCH_IS_BIG_ENDIAN
#  define BYTE_ORDER (ARCH_IS_BIG_ENDIAN ? 1 : -1)
#else
#  define BYTE_ORDER 0
#endif

#define T_MASK ((md5_word_t)~0)
#define T1 /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2 /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5 /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8 /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)

static void
md5_process(md5_state_t *pms, const md5_byte_t *data /*[64]*/)
{
    md5_word_t
        a = pms->abcd[0], b = pms->abcd[1],
        c = pms->abcd[2], d = pms->abcd[3];
    md5_word_t t;
#if BYTE_ORDER > 0
    /* Define storage only for big-endian CPUs. */
    md5_word_t X[16];
#else
    /* Define storage for little-endian or both types of CPUs. */
    md5_word_t xbuf[16];
    const md5_word_t *X;
#endif

    {
#if BYTE_ORDER == 0
        /*
         * Determine dynamically whether this is a big-endian or
         * little-endian machine, since we can use a more efficient
         * algorithm on the latter.
         */
        static const int w = 1;

        if (*((const md5_byte_t *)&w)) /* dynamic little-endian */
#endif
#if BYTE_ORDER <= 0             /* little-endian */
        {
            /*
             * On little-endian machines, we can process properly aligned
             * data without copying it.
             */
            if (!((data - (const md5_byte_t *)0) & 3)) {
                /* data are properly aligned */
                X = (const md5_word_t *)data;
            } else {
                /* not aligned */
                memcpy(xbuf, data, 64);
                X = xbuf;
            }
        }
#endif
#if BYTE_ORDER == 0
        else                    /* dynamic big-endian */
#endif
#if BYTE_ORDER >= 0             /* big-endian */
        {
            /*
             * On big-endian machines, we must arrange the bytes in the
             * right order.
             */
            const md5_byte_t *xp = data;
            int i;

#  if BYTE_ORDER == 0
            X = xbuf;           /* (dynamic only) */
#  else
#    define xbuf X              /* (static only) */
#  endif
            for (i = 0; i < 16; ++i, xp += 4)
                xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
        }
#endif
    }

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* Round 1. */
    /* Let [abcd k s i] denote the operation
       a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + F(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  7,  T1);
    SET(d, a, b, c,  1, 12,  T2);
    SET(c, d, a, b,  2, 17,  T3);
    SET(b, c, d, a,  3, 22,  T4);
    SET(a, b, c, d,  4,  7,  T5);
    SET(d, a, b, c,  5, 12,  T6);
    SET(c, d, a, b,  6, 17,  T7);
    SET(b, c, d, a,  7, 22,  T8);
    SET(a, b, c, d,  8,  7,  T9);
    SET(d, a, b, c,  9, 12, T10);
    SET(c, d, a, b, 10, 17, T11);
    SET(b, c, d, a, 11, 22, T12);
    SET(a, b, c, d, 12,  7, T13);
    SET(d, a, b, c, 13, 12, T14);
    SET(c, d, a, b, 14, 17, T15);
    SET(b, c, d, a, 15, 22, T16);
#undef SET

     /* Round 2. */
     /* Let [abcd k s i] denote the operation
          a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + G(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  1,  5, T17);
    SET(d, a, b, c,  6,  9, T18);
    SET(c, d, a, b, 11, 14, T19);
    SET(b, c, d, a,  0, 20, T20);
    SET(a, b, c, d,  5,  5, T21);
    SET(d, a, b, c, 10,  9, T22);
    SET(c, d, a, b, 15, 14, T23);
    SET(b, c, d, a,  4, 20, T24);
    SET(a, b, c, d,  9,  5, T25);
    SET(d, a, b, c, 14,  9, T26);
    SET(c, d, a, b,  3, 14, T27);
    SET(b, c, d, a,  8, 20, T28);
    SET(a, b, c, d, 13,  5, T29);
    SET(d, a, b, c,  2,  9, T30);
    SET(c, d, a, b,  7, 14, T31);
    SET(b, c, d, a, 12, 20, T32);
#undef SET

     /* Round 3. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + H(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  5,  4, T33);
    SET(d, a, b, c,  8, 11, T34);
    SET(c, d, a, b, 11, 16, T35);
    SET(b, c, d, a, 14, 23, T36);
    SET(a, b, c, d,  1,  4, T37);
    SET(d, a, b, c,  4, 11, T38);
    SET(c, d, a, b,  7, 16, T39);
    SET(b, c, d, a, 10, 23, T40);
    SET(a, b, c, d, 13,  4, T41);
    SET(d, a, b, c,  0, 11, T42);
    SET(c, d, a, b,  3, 16, T43);
    SET(b, c, d, a,  6, 23, T44);
    SET(a, b, c, d,  9,  4, T45);
    SET(d, a, b, c, 12, 11, T46);
    SET(c, d, a, b, 15, 16, T47);
    SET(b, c, d, a,  2, 23, T48);
#undef SET

     /* Round 4. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + I(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  6, T49);
    SET(d, a, b, c,  7, 10, T50);
    SET(c, d, a, b, 14, 15, T51);
    SET(b, c, d, a,  5, 21, T52);
    SET(a, b, c, d, 12,  6, T53);
    SET(d, a, b, c,  3, 10, T54);
    SET(c, d, a, b, 10, 15, T55);
    SET(b, c, d, a,  1, 21, T56);
    SET(a, b, c, d,  8,  6, T57);
    SET(d, a, b, c, 15, 10, T58);
    SET(c, d, a, b,  6, 15, T59);
    SET(b, c, d, a, 13, 21, T60);
    SET(a, b, c, d,  4,  6, T61);
    SET(d, a, b, c, 11, 10, T62);
    SET(c, d, a, b,  2, 15, T63);
    SET(b, c, d, a,  9, 21, T64);
#undef SET

     /* Then perform the following additions. (That is increment each
        of the four registers by the value it had before this block
        was started.) */
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

#ifdef __cplusplus
extern "C" void
#else
void
#endif
md5_init(md5_state_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
    pms->abcd[3] = 0x10325476;

    memset(pms->buf, 0, sizeof(pms->buf));
}

#ifdef __cplusplus
extern "C" void
#else
void
#endif
md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes)
{
    const md5_byte_t *p = data;
    int left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    md5_word_t nbits = (md5_word_t)(nbytes << 3);

    if (nbytes <= 0)
        return;

    /* Update the message length. */
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
        pms->count[1]++;

    /* Process an initial partial block. */
    if (offset) {
        int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

        memcpy(pms->buf + offset, p, copy);
        if (offset + copy < 64)
            return;
        p += copy;
        left -= copy;
        md5_process(pms, pms->buf);
    }

    /* Process full blocks. */
    for (; left >= 64; p += 64, left -= 64)
        md5_process(pms, p);

    /* Process a final partial block. */
    if (left)
        memcpy(pms->buf, p, left);
}

#ifdef __cplusplus
extern "C" void
#else
void
#endif
md5_finish(md5_state_t *pms, md5_byte_t digest[16])
{
    static const md5_byte_t pad[64] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    md5_byte_t data[8];
    int i;

    /* Save the length before padding. */
    for (i = 0; i < 8; ++i)
        data[i] = (md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    /* Pad to 56 bytes mod 64. */
    md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    /* Append the length. */
    md5_append(pms, data, 8);
    for (i = 0; i < 16; ++i)
        digest[i] = (md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

#ifndef COVERITY_COMPILE_AS_C
static void internal_md5_encode
(const void *s,
 size_t len,
 md5_byte_t *digest) {
    md5_state_t st;
    md5_init(&st);
    md5_append(&st, (md5_byte_t*)s, len);
    md5_finish(&st, digest);
}

static void internal_md5_pretty_string
(const md5_byte_t *digest,
 md5_pretty_string &md5_value) {
    md5_value = hex_buffer_string((const char *)digest, MD5_HASH_SIZE);
}

// string versions
void md5_encode(const void *buf, unsigned len, md5_pretty_string &out_value)
{
    md5_byte_t digest[MD5_HASH_SIZE];
    internal_md5_encode(buf, len, digest);
    internal_md5_pretty_string(digest, out_value);
}

void md5_encode(const VectorBase<char> &buf, md5_pretty_string &out_value)
{
    md5_encode(buf.data(), buf.size(), out_value);
}

void md5_encode(const VectorBase<unsigned char> &buf, md5_pretty_string &out_value)
{
    md5_encode(buf.data(), buf.size(), out_value);
}

void md5_encode(const char *s, md5_pretty_string &out_value)
{
    md5_encode(s, strlen(s), out_value);
}

void md5_encode(const string &buf, unsigned off, unsigned len, md5_pretty_string &out_value)
{
    md5_encode(buf.data() + off, len, out_value);
}

// md5_pair_t versions
void md5_encode(const void *buf, unsigned len, md5_pair_t &md5pair)
{
    md5_byte_t digest[MD5_HASH_SIZE];
    internal_md5_encode(buf, len, digest);

    packed_md5_to_pair((const char *)digest, md5pair);
}

void md5_encode(const VectorBase<char> &buf, md5_pair_t &out_value)
{
    md5_encode(buf.data(), buf.size(), out_value);
}

void md5_encode(const VectorBase<unsigned char> &buf, md5_pair_t &out_value)
{
    md5_encode(buf.data(), buf.size(), out_value);
}

void md5_encode(const char *s, md5_pair_t &out_value)
{
    md5_encode(s, strlen(s), out_value);
}

void md5_encode(const string &buf, unsigned off, unsigned len, md5_pair_t &out_value)
{
    md5_encode(buf.data() + off, len, out_value);
}

// md5_digest_as_string versions
void md5_encode_raw(const void *buf, unsigned len, md5_digest_as_string &out_value)
{
    md5_byte_t digest[MD5_HASH_SIZE];
    internal_md5_encode(buf, len, digest);
    out_value.clear();
    out_value.append((const char*)digest, sizeof(digest));
}

void md5_encode_raw(const VectorBase<char> &buf, md5_digest_as_string &out_value)
{
    md5_encode_raw(buf.data(), buf.size(), out_value);
}

void md5_encode_raw(const VectorBase<unsigned char> &buf, md5_digest_as_string &out_value)
{
    md5_encode_raw(buf.data(), buf.size(), out_value);
}

void md5_encode_raw(const char *s, md5_digest_as_string &out_value)
{
    md5_encode_raw(s, strlen(s), out_value);
}

void md5_encode_raw(const string &buf, unsigned off, unsigned len, md5_digest_as_string &out_value)
{
    md5_encode_raw(buf.data() + off, len, out_value);
}

md5_pair_t md5_hash(string const &data)
{
    md5_pair_t ret;
    md5_encode(data, ret);
    return ret;
}


#define BLOCKSZ (64 * 1024)
// Compute the md5 of an entire file.
int md5_encode_file(const Filename& file_path,
                    md5_pretty_string &md5_value)
{
    static unsigned char buf[BLOCKSZ];
    if(!file_exists(file_path))
        return 0;
    eifstream file;
    try {
        file.open(file_path, eifstream::binary);
    } catch(file_does_not_exist_error &e) {
        COV_CAUGHT(e);
        return 0;
    }

    md5_state_t state;
    md5_init(&state);

    do {
        file.read((char *)buf, BLOCKSZ);
        md5_append(&state, buf, file.gcount());
    } while(file.gcount() == BLOCKSZ);

    md5_byte_t digest[MD5_HASH_SIZE];
    md5_finish(&state, digest);

    internal_md5_pretty_string(digest, md5_value);

    return 1;
}

md5stream::md5streambuf::md5streambuf(int buf_size) {
    init(buf_size);
}

md5stream::md5streambuf::md5streambuf() {
    init(sizeof(buf));
}

void md5stream::md5streambuf::init(int size) {
    buf_size = size;
    md5_init(&md5state);
    // Keep 1 character for overflow
    setp(buf, buf + buf_size - 1);
}

md5stream::md5streambuf::~md5streambuf() {
}

streamsize md5stream::md5streambuf::xsputn(const char_type *str, streamsize count) {
    if(count > buf_size * 2) {
        // If it's going to be big, use md5_append directly.
        flush_buf();
        md5_append(&md5state, (const md5_byte_t *)str, count);
        return count;
    } else {
        // Otherwise rely on the normal "overflow" behavior.
        return streambuf::xsputn(str, count);
    }
}

void md5stream::md5streambuf::flush_buf() {
    md5_append(&md5state, (const md5_byte_t *)pbase(), pptr() - pbase());
    setp(buf, buf + buf_size - 1);
}

int md5stream::md5streambuf::overflow (int ch) {
    if(traits_type::eq_int_type(ch, traits_type::eof())) {
        flush_buf();
        return ch;
    }
    // We reserved 1 slot in the buffer for overflow.
    *pptr() = ch;
    pbump(1);
    flush_buf();
    return ch;
}

string md5stream::md5streambuf::pretty_md5() {
    char digest[MD5_HASH_SIZE];
    raw_md5(digest);
    return hex_buffer_string(digest, MD5_HASH_SIZE);
}

void md5stream::md5streambuf::raw_md5(char digest[MD5_HASH_SIZE]) {
    flush_buf();
    md5_finish(&md5state, (md5_byte_t *)digest);
    md5_init(&md5state);
}

void md5stream::raw_md5(md5_pair_t &digest) {
    md5_byte_t digest_bytes[MD5_HASH_SIZE];

    // Get the 128-bit digest as 16 bytes in big-endian order.
    raw_md5((char*)digest_bytes);

    // Store the packed representation as two 64-bit integers
    packed_md5_to_pair((char*)digest_bytes, digest);
}

void packed_md5_to_pair(const char *bytes, md5_pair_t &digest)
{
    unsigned long long hi = 0, lo = 0;
    const md5_byte_t* digest_bytes = (const md5_byte_t*)bytes;

    // Synthesize the high 64-bit integer.
    for(unsigned i = 0; i < 7; ++i) {
        hi |= digest_bytes[i];
        hi <<= 8;
    }
    hi |= digest_bytes[7];

    // Synthesize the low 64-bit integer.
    for(unsigned i = 8; i < 15; ++i) {
        lo |= digest_bytes[i];
        lo <<= 8;
    }
    lo |= digest_bytes[15];

    digest.hi = hi;
    digest.lo = lo;
}

md5_digest_as_string pair_to_packed_md5(md5_pair_t const &digest)
{
    char buf[MD5_HASH_SIZE];
    pair_to_packed_md5(digest, buf);
    return string(buf, sizeof(buf));
}

void pair_to_packed_md5(
    md5_pair_t const &digest,
    void *vbuf
)
{
    char *rv = (char *)vbuf;
    unsigned long long hi = digest.hi, lo = digest.lo;

    // Synthesize the high 64-bit integer.
    int i = 15;
    for(; i >= 8; --i) {
        rv[i] = lo & 0xff;
        lo >>= 8;
    }
    for(; i >= 0; --i) {
        rv[i] = hi & 0xff;
        hi >>= 8;
    }
}


static int fromHexDigit(char c) {
    switch (tolower(c)) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
    default: assert(!"non-hexadecimal character"); return 0;
    }
}

void md5_str_to_pair(const string &str, md5_pair_t &digest) {
    digest.from_pretty_string(str);
}

void md5_pair_t::from_pretty_string(const blob_t &str)
{
    cond_assert(str.size() == MD5_HASH_SIZE * 2);
    char md5buf[MD5_HASH_SIZE];
    memset(md5buf, 0, sizeof(md5buf));

    for (int i = 0; i < 32; i++) {
        md5buf[i / 2] |= fromHexDigit(str[i]) << (4 * ((i+1) % 2));
    }
    from_digest(md5buf);
}

bool looks_like_md5(const string &str) {
    return ((str.size() == (MD5_HASH_SIZE * 2)) && str.find_first_not_of("ABCDEFabcdef0123456789") == string::npos);
}


static char toHexDigit(int val)
{
    assert(0 <= val && val < 16);
    if (val < 10) {
        return '0' + val;
    }
    else {
        return 'a' + (val-10);
    }
}

static void render_64bit_as_hex(char *dest, long long value)
{
    STATIC_ASSERT(MD5_HASH_SIZE == 16);
    STATIC_ASSERT(sizeof(value) >= 8);

    for (int i=0; i<16; i++) {
        // use unsigned value so right shifts won't sign-extend
        unsigned long long tmp = (unsigned long long)value;

        // select the i'th 4-bits, starting with the highest
        tmp >>= (15-i) * 4;
        tmp &= 0xF;

        // render as ascii-hex
        dest[i] = toHexDigit((int)tmp);
    }
}

string md5_pair_to_str(md5_pair_t const &digest)
{
    return digest.to_pretty_string();
}

void md5_pair_t::to_pretty_string_buf(char *buf) const
{
    render_64bit_as_hex(buf, hi);
    render_64bit_as_hex(buf + MD5_HASH_SIZE, lo);
}

md5_pretty_string md5_pair_t::to_pretty_string() const
{
    char buf[MD5_HASH_SIZE*2];
    to_pretty_string_buf(buf);
    return string(buf, ARRAY_SIZE(buf));
}

void md5_pair_t::out_as_pretty_string(ostream &out) const
{
    char buf[MD5_HASH_SIZE*2];
    to_pretty_string_buf(buf);
    out.write(buf, ARRAY_SIZE(buf));
}

ostream &operator<<(ostream &out, const md5_pair_t &md5pair)
{
    md5pair.out_as_pretty_string(out);
    return out;
}

md5_digest_as_string md5_pair_t::to_digest() const
{
    return pair_to_packed_md5(*this);
}

void md5_pair_t::to_digest(void *digest) const
{
    pair_to_packed_md5(*this, digest);
}

void md5_pair_t::from_digest(const blob_t &digest)
{
    cond_assert(digest.size() == MD5_HASH_SIZE);
    from_digest(digest.data());
}

void md5_pair_t::from_digest(const void *digest)
{
    packed_md5_to_pair((const char *)digest, *this);
}

void md5_pair_t::from_data(const blob_t &buf)
{
    md5_encode(buf, *this);
}

void md5_pair_t::from_data(const VectorBase<unsigned char> &buf)
{
    md5_encode(buf, *this);
}

int md5_pair_t::compare(const md5_pair_t &other) const
{
    if(hi != other.hi) {
        return hi < other.hi ? -1 : 1;
    }
    if(lo != other.lo) {
        return lo < other.lo ? -1 : 1;
    }
    return 0;
}

size_t md5_pair_t::compute_hash(size_t h) const
{
    h = cov_hash_int64(hi, h);
    h = cov_hash_int64(lo, h);
    return h;
}

size_t md5_pair_t::compute_hash() const
{
    return compute_hash(COV_HASH_INIT);
}

// unit tests
void md5stream::unit_tests()
{
    const string src = "Test String";
    const md5_pair_t srcHashPair(
        0xbd08ba3c982eaad7LL,
        0x68602536fb8e1184LL
    );
    const string srcHashPretty("bd08ba3c982eaad768602536fb8e1184");
    const string srcHashRaw("\xbd\x08\xba\x3c\x98\x2e\xaa\xd7\x68\x60\x25\x36\xfb\x8e\x11\x84");

    cond_assert(md5_hash(src) == srcHashPair);
    cond_assert(looks_like_md5(srcHashPretty));
    cond_assert(!looks_like_md5(srcHashPretty + 'a'));
    string badmd5 = srcHashPretty;
    badmd5[0] = 'g';
    cond_assert(!looks_like_md5(badmd5));
    cond_assert(pair_to_packed_md5(srcHashPair) == srcHashRaw);
    char digest[MD5_HASH_SIZE];
    pair_to_packed_md5(srcHashPair, digest);
    cond_assert(!memcmp(digest, srcHashRaw.data(), MD5_HASH_SIZE));

    // Test md5stream::md5streambuf::pretty_md5()
    {
        md5stream md5str(3);
        md5str << src.substr(0, 4) << src.substr(4);

        const string md5 = md5str.pretty_md5();
        cond_assert(md5 == srcHashPretty);
    }

    // Test md5stream::raw_md5(pair&) [and packed_md5_to_pair() indirectly]
    {
        md5stream md5str;
        md5str << src;

        md5_pair_t md5_pair;
        md5str.raw_md5(md5_pair);
        cond_assert(md5_pair == srcHashPair);
    }

    {
        md5_pretty_string md5Pretty;
        md5_encode(src, md5Pretty);
        cond_assert(md5Pretty == srcHashPretty);
        md5_encode(src.c_str(), md5Pretty);
        cond_assert(md5Pretty == srcHashPretty);
        md5_encode(src, 0, src.size(), md5Pretty);
        cond_assert(md5Pretty == srcHashPretty);
        md5_encode(src.data(), src.size(), md5Pretty);
        cond_assert(md5Pretty == srcHashPretty);
        md5_encode(VectorBase<unsigned char>((unsigned char *)src.data(), src.size()), md5Pretty);
        cond_assert(md5Pretty == srcHashPretty);
    }

    {
        md5_pair_t md5Pair;
        md5_encode(src, md5Pair);
        cond_assert(md5Pair == srcHashPair);
        md5_encode(src.c_str(), md5Pair);
        cond_assert(md5Pair == srcHashPair);
        md5_encode(src, 0, src.size(), md5Pair);
        cond_assert(md5Pair == srcHashPair);
        md5_encode(src.data(), src.size(), md5Pair);
        cond_assert(md5Pair == srcHashPair);
        md5_encode(VectorBase<unsigned char>((unsigned char *)src.data(), src.size()), md5Pair);
        cond_assert(md5Pair == srcHashPair);
    }

    {
        md5_digest_as_string md5Raw;
        md5_encode_raw(src, md5Raw);
        cond_assert(md5Raw == srcHashRaw);
        md5_encode_raw(src.c_str(), md5Raw);
        cond_assert(md5Raw == srcHashRaw);
        md5_encode_raw(src, 0, src.size(), md5Raw);
        cond_assert(md5Raw == srcHashRaw);
        md5_encode_raw(src.data(), src.size(), md5Raw);
        cond_assert(md5Raw == srcHashRaw);
        md5_encode_raw(VectorBase<unsigned char>((unsigned char *)src.data(), src.size()), md5Raw);
        cond_assert(md5Raw == srcHashRaw);
    }

    // Test md5_str_to_pair() and inverse
    {
        char const *testStr = "0123456789abcdeffedcba9876543210";
        md5_pair_t md5_pair;
        md5_pair.from_pretty_string(asciizToBlob(testStr));
        assert(md5_pair.hi  == (long long)0x0123456789abcdefLL &&
               md5_pair.lo == (long long)0xfedcba9876543210LL);
        assert(str_equal(md5_pair.to_pretty_string(), testStr));
    }
}

void md5_unit_tests()
{
    md5stream::unit_tests();
}
#endif // COVERITY_COMPILE_AS_C
