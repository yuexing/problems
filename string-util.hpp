#ifndef HEX_HPP
#define HEX_HPP

// wrap things in hex way.
int stricmp(const char *str1, const char *str2)
{
   retrun strincmp(str1, str2, INT_MAX); 
}

int strincmp(const char *str1, const char *str2, int n)
{
    for(int i = 0; i < n; i++) {
        char c1 = tolower(str1[i]), 
             c2 = tolower(str2[i]);
        if(c1 == 0 && c2 == 0) {
            return 0;
        } else if( c1 != c2 ){
            return c1 - c2;
        }
    }
    return 0; // unreachable
}

inline bool str_equal(const string &s1, const char *s2) {
    return s2 && s1 == s2;
}
inline bool str_equal(const char *s1, const string &s2) {
    return s1 && s1 == s2;
}
inline bool str_equal(const string &s1, const string &s2) {
    return s1 == s2;
}

// Equality of strings - ignoring case
inline bool str_equali(const char *a1, const char *a2) {
  if(a1 == a2)
    return true;
  if(!a1 || !a2)
    return false;
  return stricmp(a1, a2);
}

// Substring comparison
// does a begin with b
bool begins_with(const string &a, const string &b)
{
    if(a.size() < b.size()) return false;
    return a.compare(0, b.size(), b) == 0;
}
// does a end with b
bool ends_with(const string &a, const string &b)
{
    if(a.size() < b.size()) return false;
    return a.compare(a.size() - b.size(), b.size(), b) == 0;
}
// Same as above, except with const char *
// Faster than having to create strings
bool begins_with(const char *a, const char *b)
{
    return memcmp(a, b, strlen(b)) == 0;
}


// Return true if 'needle' is a substring (case sensitive) of 'haystack'.
// TODO: implement with KMP
bool has_substring(const string &haystack, const string &needle);

// String replacements

// Modifies s by replacing all occurrences of 'a' with 'b',
// and returns the number of replacements made.
int replace_all(string &s, const char *a, const char *b);

// Like 'replace_all' except case-insensitive replacement, mostly
// for paths on windows.
int replace_alli(string &s, const char *a, const char *b);

// Like 'replace_all' but only replaces last occurrence.
// No effect or failure indication if 'a' not found.
void replace_last(string &s, const char *a, const char *b);

/**
 * Removes trailing newlines ('\n'). Modifies the string in place, and returns it.
 **/
string &chomp(string &s);

// Remove initial and trailing whitespace characters from string.
// Modifies string in place.
string &trim(string &s);

// it's unforunate that 'trim' already exists and has the wrong
// semantics; in-place means that I can't use it functionally; so
// make another with a similar name...
string trimWS(rostring s);      // *not* in-place

string bool_to_string(bool x);

bool string_to_bool(const string &str);

// Convert an integer to a string representation
string int_to_string(sint64 x);
// Same, for an unsigned number (only difference is if the number
// overflows sint64)
string uint_to_string(uint64 x);
// Same as the above two, with explicit signedness and radix.
// If "include_prefix" is true, then checks that radix is any of 2, 8,
// 10 or 16, and prefix "0x", "0", "" or "0x", respectively (unless
// it's 0, which is always represented as '0'). asserts if the radix
// is not in this list and prefix is requested.
string full_int_to_string(
    sint64 x,
    int radix,
    bool include_prefix,
    bool is_unsigned);

// Possible errors from full_string_to_int
enum StrToIntErr {
    // Valid number.
    STI_OK,
    // Valid number, followed by a non-digit character. The number
    // will still be stored in "val".
    STI_TRAILING_CHARACTER,
    // Represents a number that overflows the representable range
    STI_OVERFLOW,
    // Doesn't represent a valid number (that includes an empty string)
    STI_NOT_A_NUMBER,
};

/**
 * Turn a string into an integer, with full error checking.
 * str = string to convert
 * endptr = pointer to the first non-numeric (according to the radix)
 * character in the string, if returning STI_TRAILING_CHARACTER, or to
 * the digit that caused the overflow, if returning
 * STI_OVERFLOW. Points to the end of the string if returning STI_OK.
 * radix = base (as int strtol)
 * The special values "radixForPrefixExcludingOctal" and
 * "radixForPrefixIncludingOctal" mean that we should look for a
 * prefix to indicate the radix.
 * '0x' => 16
 * '0' => 8 (10 if "ExcludingOctal")
 * '0b' => 2
 * Anything else => 10
 * If "radix" is specified and the prefix for it is present, it will
 * be silently skipped.
 * is_unsigned: Consider we're parsing an unsigned 64-bit
 * integer. Negative numbers will cause an overflow error.
 * val = receives the value. Can be modified even if it doesn't return STI_OK.
 **/
StrToIntErr full_string_to_int(
    const char *str,
    const char *&endptr,
    int radix,
    bool is_unsigned,
    sint64 &val);

//Convert a numeric string representation to a string repesentating the
//equivalent integer value.
//
//Returns empty string on failed conversion from string to number.
string normalize_numeric_to_int_string(const char *str);

// See description of full_string_to_int.
extern int const radixForPrefixExcludingOctal;
extern int const radixForPrefixIncludingOctal;

// Turns a string into a number.
// Throws an XParse if "str" doesn't represent a number and
// "throwOnError" is true. throwOnError defaults to false for
// historical reasons.
// calls full_string_to_int with "radixForPrefixExcludingOctal",
// because it may be used for user input, where it would be confusing
// for "010" to mean 8.
long long string_to_int(const string &str, bool throwOnError = false);
// Same, with a const char *.
long long string_to_int(const char *str, bool throwOnError = false);

inline string null_to_empty_string(const char *s)
{
    return s ? string(s) : string();
}

// Representation of a floating point number, up to IEEE 128-bit
// format.
// Allows serialization as binary and text.
struct IEEE_128_bit_float {
    // Maintissa, in big-endian format.
    // Number is (1 + <mantissa>/2^112) *2^exponent except for special
    // cases (see Kind below); in which case the mantissa and exponent
    // are undefined.
    unsigned char mantissa[14];
    // Exponent (unbiased)
    int exponent;
    // Sign bit.
    bool negative;

    // Special kinds of floats
    enum Kind {
        // A normal float
        FK_NORMAL,
        // Zero (+ or -)
        FK_ZERO,
        // Infinity
        FK_INF,
        // Not-a-number
        FK_NAN
    } kind;

    // Functions
    IEEE_128_bit_float();

    // Parameters for a floating point number
    struct Parms {
        int exponent_bits;
        int mantissa_bits;
        bool has_norm_bit;
        bool bigEndian;

        // Number of bytes in the float
        size_t bytes() const;
    };
    // Parameters for host types
    static Parms hostFloatParms;
    static Parms hostDoubleParms;
    static Parms hostLongDoubleParms;

    // Read from an IEEE buffer with given parameters
    // has_norm_bit indicates whether the leading "1." in the fraction
    // part is present or not in the mantissa.
    // exponent_bits should be < 32 and mantissa_bits will be
    // truncated to 64 (65 with has_norm_bit)
    void fromIEEE(
        unsigned char const *buf,
        const Parms &parms
    );
    // Converse of the above.
    void toIEEE(
        unsigned char *buf,
        const Parms &parms
    ) const;

    // Conversion from/to host float.
    float toFloat() const;
    void fromFloat(float);
    // Conversion from/to host double.
    double toDouble() const;
    void fromDouble(double);
    // Conversion from/to host long double.
    void fromLongDouble(long double);
    long double toLongDouble() const;

    // String representation.
    // The format is:
    // -?1.<hex>.2^<n> (<n> is decimal)
    // Which corresponds closely to the IEEE representation of a floating
    // point number, and gives a human reader an idea of the real value.
    // Writing and especially pasing a decimal representation is very
    // difficult, and although there are standard libraries that are
    // supposed to be able to do it, they seldom work.
    void fromString(const string &);
    string toString() const;
    // Not-a-number
    bool isNaN() const;
    // infinity
    bool isInf() const;
    // zero (+ or -)
    bool isZero() const;

    int compare(const IEEE_128_bit_float &other) const;

    COMPARISON_OPERATOR_METHODS(IEEE_128_bit_float);

private:
    void toIEEEBigEndian(
        unsigned char *buf,
        const Parms &parms
    ) const;
};

// Copy "count" bits from "from" starting at offset "from_offset" to
// "to" starting at offset "to_offset", and update the values to point
// at the first bit after the copied bits, with from_offset and
// to_offset in [0; CHAR_BIT). "to" must have enough space in it.
// Starts with most significant bits.
// "from_offset" and "to_offset" may be >= CHAR_BIT.
void bitcpy(
    const unsigned char *&from,
    int &from_offset,
    unsigned char *&to,
    int &to_offset,
    int count
);

// Turn a long double into text (using IEEE_128_bit_float::toString)
string long_double_to_string(long double d);
// This is the converse of the above.
long double string_to_long_double(const string &);

// Wrapper for a buffer that, when output to an ostream, dumps it in
// hex format.
struct hex_buffer_t {
    hex_buffer_t(const char *buf, const char *end,
                 bool upcase):
        buf(buf), end(end), upcase(upcase) {}

    const char *buf;
    const char *end;
    bool const upcase;
};

static inline char hex_digit(unsigned char d) {
    return (d >= 10) ? (d - 10 + 'a') : (d + '0');
}

// Hexadecimal digit, lowercase or uppercase based on "upcase" flag.
char hex_digit_anycase(unsigned char d, bool upcase);

ostream &operator<<(ostream &out, const hex_buffer_t &h);

// Helpers to create a hex_buffer_t. Lowercase.
hex_buffer_t hex_buffer(const string &str);
hex_buffer_t hex_buffer(const char *buf, const char *end);
hex_buffer_t hex_buffer(const void *buf, size_t len);

// Equivalent to stringb(hex_buffer(buf, len)), but potentially more efficient.
string hex_buffer_string(const char *buf, size_t len);

// Helper structure for hex_int below
struct hex_int_t {
    hex_int_t(unsigned long long l, int width, bool upcase):
        l(l),
        width(width),
        upcase(upcase) {}

    // Value to print
    unsigned long long const l;
    // Min number of digits
    int const width;
    // Case
    bool const upcase;
};

// This allows to write:
// cout << hex_int(i)
// and doesn't rely on stream state (hex/dec manipulators)
inline hex_int_t hex_int(unsigned long long l) {
    return hex_int_t(l, /*width*/1, /*upcase*/false);
}

inline hex_int_t min_width_hex_int(unsigned long long l, int width) {
    return hex_int_t(l, width, /*upcase*/false);
}

ostream &operator<<(ostream &out, const hex_int_t &h);

// Same as hex_int but prints in binary
struct binary_int_t {
    binary_int_t(unsigned long long l, int width):
        l(l),
        width(width) {}

    // Value to print
    unsigned long long const l;
    // Min number of digits
    int const width;
};

// This allows to write:
// cout << binary_int(i)
inline binary_int_t binary_int(unsigned long long l) {
    return binary_int_t(l, /*width*/1);
}

inline binary_int_t min_width_binary_int(unsigned long long l, int width) {
    return binary_int_t(l, width);
}

ostream &operator<<(ostream &out, const binary_int_t &h);

// Helper structure for min_width_int below
struct min_width_int_t {
    min_width_int_t(sint64 l, int width, bool isUnsigned):
        l(l),
        width(width),
        isUnsigned(isUnsigned) {}

    // Value to print
    sint64 const l;
    // Min number of digits
    int const width;
    // Whether to interpret "l" as signed or unsigned.
    bool const isUnsigned;
};

// This allows to write:
// cout << min_width_uint(i, 2)  // may print e.g. '01'
// and doesn't rely on stream state (setfill/setw manipulators)
inline min_width_int_t min_width_uint(uint64 l, int width) {
    return min_width_int_t(l, width, /*isUnsigned*/true);
}
inline min_width_int_t min_width_sint(sint64 l, int width) {
    return min_width_int_t(l, width, /*isUnsigned*/false);
}

ostream &operator<<(ostream &out, const min_width_int_t &m);

struct percentage_t {
    percentage_t(
        long long numerator,
        long long denominator,
        bool includeValues):
        numerator(numerator),
        denominator(denominator),
        includeValues(includeValues) {
    }

    // See "percentage" below for a description
    long long const numerator;
    long long const denominator;
    bool const includeValues;
};

// Allow printing a percentage in "<int>.<2 digits>%" format, without the
// need to mess with iomanip.
// Prints "<invalid>%" if "denominator" is 0.
// Will work as long as numerator * 10000 doesn't overflow "long"
// If "includeValues" is true, prints in format:
// N / D (x.xx%)
// or just N / D if D is 0.
inline percentage_t percentage
(long long numerator,
 long long denominator,
 bool includeValues
) {
    return percentage_t(numerator, denominator, includeValues);
}

ostream &operator<<(ostream &out, const percentage_t &pct);


// Render 'numer' as a string, after dividing by '10^denomDigits'.
// The decimal portion will always use exactly 'denomDigits' digits.
// The integer portion will always have at least one digit, even if it
// is zero.
//
// (This doesn't rely on floating point due to its imprecision.)
string fixedPointToString(sint64 numer, int denomDigits);


// Lower/upper case the string
void makeLowerCase(string &s);
string lowerCased(const char *s);
string lowerCased(const string &s);
void makeUpperCase(string &s);
string upperCased(const char *s);
string upperCased(const string &s);


/**
 * Truncate the string to the given length, if necessary replacing the
 * last 3 characters with "..."
 * This functions used to be called just "truncate" but there's a
 * POSIX "truncate" function that truncates a file that also takes a
 * string and an integer
 **/
string truncateText(rostring s, int len);

// Converts all whitespace-only blocks (" \t\n\r") to a single space.
// Also trims leading/trailing whitespace.
string spaceNormalize(const string& s);

#if defined(__MC_MINGW__)
// string/wstring conversion
// Lifted from capture-win.cpp
// Reason: need to use this functionality in cov-cygwin.cpp
string wstring_to_string(wstring const &src);
wstring string_to_wstring(string const &src);
string make_string_take_ownership(char const *src);
wstring safe_widen_string(const char *s, unsigned len);
wstring make_wstring_take_ownership(wchar_t const *src);
string safe_narrow_string(const wchar_t *s, unsigned len);

#endif // __MC_MINGW__

// BZ 30661: Filter out comments and BOM from TLH file contents.
string skip_comments_and_bom(const string &contents);

// Parses a string of the form "1.2.3...", returning the major
// and minor version numbers. If the string is not a legal version
// string then false is returned.
bool string_to_version(const string &str, long long &major, long long &minor);

// direct string replacement, replacing instances of oldstr with newstr
// (newstr may be "")
// XXX: this uses C strings internally, so will truncate at the
// first nul character.
string replace(rostring src, rostring oldstr, rostring newstr);

// works like unix "tr": the source string is translated character-by-character,
// with occurrences of 'srcchars' replaced by corresponding characters from
// 'destchars'; further, either set may use the "X-Y" notation to denote a
// range of characters from X to Y
string translate(rostring src, rostring srcchars, rostring destchars);

// a simple example of using translate; it was originally inline, but a bug
// in egcs made me move it out of line
string stringToupper(rostring src);
//  { return translate(src, "a-z", "A-Z"); }


// remove any whitespace at the beginning or end of the string
// Note: trimWhitespace uses 'isspace' while trimWS/trim use
// [ \t\r\n]
string trimWhitespace(rostring str);
// get the first identifier (as in C identifier) in the string
string firstIdentifier(rostring str);


// return 'prefix', pluralized if n!=1; for example
//   plural(1, "egg") yields "egg", but
//   plural(2, "egg") yields "eggs";
// it knows about a few irregular pluralizations (see the source),
// and the expectation is I'll add more irregularities as I need them
string plural(int n, rostring prefix);

// same as 'plural', but with the stringized version of the number:
//   pluraln(1, "egg") yields "1 egg", and
//   pluraln(2, "egg") yields "2 eggs"
string pluraln(int n, rostring prefix);

// Pluralizes/singularizes a verb.
// The "verb" is the infinitive form, such as "be" or "do"
// Can also be "was"
// make => makes/make
// do => does/do
// be => is/are
// was => was/were
// have => has/have
// etc.
string plural_verb(int n, rostring verb);

// prepend with an indefinite article:
//   a_or_an("foo") yields "a foo", and
//   a_or_an("ogg") yields "an ogg"
string a_or_an(rostring noun);


// smcpeak 2014-02-15: Removed 'writeStringToFile',
// 'readStringFromFile', and 'readLine' because they were never used.


// Unfortunately, there is already a 'chomp' in text/string-utils.hpp
// (i.e. above)
// and it has slightly different (IMO wrong) semantics.  Fortunately,
// I only call this function once in all of Elsa, and it is in the C++
// parser itself, so does not affect my desired use of the AST.
// Therefore I will just disable my version and leave the one already
// in Prevent alone.
#if 0
// like perl 'chomp': remove a final newline if there is one
string chomp(rostring src);
#endif // 0


// construct a string out of characters from 'p' up to 'p+n-1',
// inclusive; resulting string length is 'n'
string substring(char const *p, int n);
inline string substring(rostring p, int n)
{ return p.substr(0, n); }


// This exists as part of an effort to provide some insulation from
// the string class implementation in Elsa.
inline char const *toCStr(rostring s)
  { return s.c_str(); }


// Compatibility layer to allow 'string' to be treated like 'char*';
// previously, Elsa used a string class that had an implicit
// conversion to 'char*' (which I still think is a good idea), so this
// layer helps that code.
inline int strlen(rostring s) { return s.length(); }
inline int strcmp(rostring s1, rostring s2) { return s1.compare(s2); }


// add spaces to the end until the string has the desired length; if the
// string is already larger, return it as-is
string padOutTo(rostring str, int desiredLength);

// Add the specified 'padChar' to the left of 'str' until it has the
// desired length.  If it is already larger, return as-is.
string addLeftPadding(rostring str, int desiredLength, char padChar);

template<class F>
string remove_all_if(const string &str, F cond){
    string res;
    string::const_iterator cit = str.begin();
    for(;cit != str.end();++cit) {
        if(cond(*cit))
            continue;
        res+=*cit;
    }
    return res;
}

#if !defined(__MC_HPUX_PA__)

// Define "strrstr" on the platforms where it isn't already defined.
// This is to "strstr" as "strrchr" is to "strchr", i.e. looks for the
// last occurrence of "needle" in "haystack".
// Not guaranteed to be efficient.
const char *strrstr(const char *haystack, const char *needle);

#endif

#endif /* HEX_HPP */
