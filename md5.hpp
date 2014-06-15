/*
  Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.

  Portions modified by Coverity, Inc.
  (c) 2004-2008,2011-2013 Coverity, Inc. All rights reserved worldwide.

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

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Removed support for non-ANSI compilers; removed
        references to Ghostscript; clarified derivation from RFC 1321;
        now handles byte order either statically or dynamically.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
        added conditionalization for C++ compilation from Martin
        Purschke <purschke@bnl.gov>.
  1999-05-03 lpd Original version.
 */

#ifndef md5_INCLUDED
#  define md5_INCLUDED

/*
 * This package supports both compile-time and run-time determination of CPU
 * byte order.  If ARCH_IS_BIG_ENDIAN is defined as 0, the code will be
 * compiled to run only on little-endian CPUs; if ARCH_IS_BIG_ENDIAN is
 * defined as non-zero, the code will be compiled to run only on big-endian
 * CPUs; if ARCH_IS_BIG_ENDIAN is not defined, the code will be compiled to
 * run on either big- or little-endian CPUs, but will run slightly less
 * efficiently on either one than if ARCH_IS_BIG_ENDIAN is defined.
 */

#include "md5-fwd.hpp"                   // this module; defines md5_pair_t

#include "text/string.hpp"
#include "text-flatten/text-flatten-fwd.hpp"
#include "flatten/flatten-fwd.hpp"
#include "containers/vector.hpp"
#include "text/iostream.hpp"
#include "file/filename-class-fwd.hpp"
#include "containers/pair.hpp"
#include "containers/container-utils.hpp"// VectorBase
#include "blob.hpp" // blob_t

// Import the C API
#include "md5.h"

// Use typedefs to make it clear which strings correspond to a digest
// (raw bytes) and which correspond to a "pretty" md5 (hex characters)
typedef string md5_digest_as_string;
typedef string md5_pretty_string;

// An md5 hash, as a pair of integers.
class md5_pair_t {
public:
    // Actual data

    // High bits (first 64 bits of the digest, interpreted as big endian)
    long long hi;
    // Low bits (second 64 bits of the digest, interpreted as big endian)
    long long lo;

    // Functions
    md5_pair_t(long long hi, long long lo): hi(hi), lo(lo) {}
    md5_pair_t(): hi(), lo() {}

    // Conversions to and from "pretty" format.
    md5_pretty_string to_pretty_string() const;
    // "buf" must have MD5_HASH_SIZE*2 bytes available. It will not be
    // nul-terminated.
    void to_pretty_string_buf(char *buf) const;
    void from_pretty_string(const blob_t &str);
    // Outputs to stream as "pretty string"
    void out_as_pretty_string(ostream &out) const;

    // Conversions to and from "digest" format.
    md5_digest_as_string to_digest() const;
    // "buf" must have MD5_HASH_SIZE bytes available.
    void to_digest(void *buf) const;
    // This will check the number of bytes
    void from_digest(const blob_t &digest);
    // This will assume there are the right number of bytes
    // (i.e. MD5_HASH_SIZE)
    void from_digest(const void *digest);

    // Construct from byte buffers (i.e. hashes the buffer)
    void from_data(const blob_t &buf);
    void from_data(const VectorBase<unsigned char> &buf);

    int compare(const md5_pair_t &other) const;
    COMPARISON_OPERATOR_METHODS(md5_pair_t);

    size_t compute_hash(size_t h) const;
    size_t compute_hash() const;

    // For dependency reasons, implemented in flatutil.cpp
    void xferFields(Flatten &flat);
    // For dependency reasons, implemented in text-flatten.cpp
    void xferFieldsText(TextFlatten &flat, const char *fieldName);
};

ostream &operator<<(ostream &out, const md5_pair_t &md5pair);

// Compute the md5 of a char buffer into a "pretty" string.
void md5_encode(const void *buf, unsigned len, md5_pretty_string &out_value);

// Utility versions
void md5_encode(const char *s, md5_pretty_string &md5_value);
void md5_encode(const VectorBase<char> &buf, md5_pretty_string &md5_value);
void md5_encode(const VectorBase<unsigned char> &buf, md5_pretty_string &md5_value);
void md5_encode(const string &buf, unsigned off, unsigned len, md5_pretty_string &out_value);

// Compute the md5 of an entire file.
// XXX: This should cause dependency issues, as it uses "eifstream"
// and "Filename" which depend on this library.
int md5_encode_file(const Filename& file_path, md5_pretty_string &out_value);

// Same as above, hashing to an md5pair
void md5_encode(const void *buf, unsigned len, md5_pair_t &md5pair);

// Utility versions
// C string
void md5_encode(const char *s, md5_pair_t &md5Pair);
// "VectorBase" (includes conversion from std::string)
void md5_encode(const VectorBase<char> &buf, md5_pair_t &md5Pair);
// Same with unsigned char
void md5_encode(const VectorBase<unsigned char> &buf, md5_pair_t &md5Pair);
// substring
void md5_encode(const string &buf, unsigned off, unsigned len, md5_pair_t &md5Pair);

static const unsigned MD5_HASH_SIZE = 16;

// Same as md5_encode, except the returned md5 is in raw bytes, not
// hex format.  It will be 16 bytes instead of 32.
void md5_encode_raw(const void *buf, unsigned  len, md5_digest_as_string &out_value);
// Utility versions
void md5_encode_raw(const char *s, md5_digest_as_string &out_value);
void md5_encode_raw(const VectorBase<char> &buf, md5_digest_as_string &out_value);
void md5_encode_raw(const VectorBase<unsigned char> &buf, md5_digest_as_string &out_value);
void md5_encode_raw(const string &buf, unsigned off, unsigned len, md5_digest_as_string &out_value);

// Hash the buffer contents and yield the result as a pair of 64-bit integers.
md5_pair_t md5_hash(string const &data);

/**
 * Returns whether \p str looks like a "pretty" MD5 (i.e. 32 hex
 * digit chars)
 **/
bool looks_like_md5(const md5_pretty_string &str);


/**
 * Reads the packed md5 bytes (in network order) and stores as a pair of
 * 64-bit integers (hi, lo).
 **/
void packed_md5_to_pair(const char *bytes, md5_pair_t &digest);

/**
 * Converse of the above.
 **/
md5_digest_as_string pair_to_packed_md5(md5_pair_t const &digest);

/**
 * Same as above, to a fixed-length buffer (which should be of size MD5_HASH_SIZE)
 **/
void pair_to_packed_md5(md5_pair_t const &digest, void *bytes);

/**
 * Converts the md5 from string format to a pair of 64-bit integers.
 *
 **/
void md5_str_to_pair(const md5_pretty_string &str, md5_pair_t &digest);

/**
 * Convert from the pair format to the string format.
 **/
md5_pretty_string md5_pair_to_str(md5_pair_t const &digest);


/**
 * A stream that computes an md5 of what's put into it.
 **/
class md5stream: public ostream {
private:
    class md5streambuf: public streambuf {
    public:
        md5streambuf(int buf_size);
        md5streambuf();
        ~md5streambuf();
        string pretty_md5();
        void raw_md5(char [MD5_HASH_SIZE]);
    protected:
        streamsize xsputn(const char_type *str, streamsize count);
        int overflow (int ch);
    private:
        void init(int buf_size);
        void flush_buf();
        md5_state_t md5state;
        // Internal buffer. We'll update the md5 state only when this
        // is full (it's faster that way).
        char buf[1024];
        // Allow customizing the buffer size, for testing.
        int buf_size;
    } buf;
    // Version that specifies the buf size, which must be smaller than
    // sizeof(md5streambuf::buf)
    // For testing.
    explicit md5stream(int buf_size):ostream(&buf), buf(buf_size) {}
public:
    static void unit_tests();
    md5stream():ostream(&buf){}
    md5_pretty_string pretty_md5(){flush();return buf.pretty_md5();}
    void raw_md5(char digest[MD5_HASH_SIZE]) {flush();buf.raw_md5(digest);}
    /**
     * Stores the digest as a pair of 64-bit integers (hi, lo).
     **/
    void raw_md5(md5_pair_t &digest);
};


void md5_unit_tests();



#endif /* md5_INCLUDED */
