#ifndef PARSE_HPP
#define PARSE_HPP
// read-only string
typedef string const &rostring;

// read the next decimal integer from and advance 'p'; throw xparse if
// 'p' does not begin with digits
long readInt(char const *&p);

// require that 's' consists only of decimal digits, and has at
// least one of them too
long readIntOnly(rostring s);

// read long long; this one tolerates a leading '-' (maybe readInt
// should too?)
long long readLongLong(char const *&p);

// similar to readIntOnly, but for long long
long long readLongLongOnly(rostring s);

// check that *p == c, and advance 'p'; throw xparse if not equal
void expect(char const *&p, char c);

// advance past 's', which must equal a prefix of 'p'
void expectString(char const *&p, char const *s);

// read the next char from 'p', advancing it; require that the read
// character be among those in 'among' (not including NUL), throwing
// XParse if it is not; return the read character
char readCharFromAmong(char const *&p, char const *among);

// the standard interface, start/n, is sort of annoying; so this
// one is start/end+1
string extractSubstring(char const *start, char const *end);

// return the end of the longest prefix of 'p' that does not have any
// of the characters in 'exclude'; equivalently, return a pointer to
// the first character in 'p' that is in 'exclude'
char const *scanUntil(char const *p, char const *exclude);

// return the string that 'scanUntil' demarks, advancing 'p' past the
// characters that are returned
string readString(char const *&p, char const *exclude);

// advance 'p' as long as isspace(*p)
void skipWhitespace(char const *&p);

// advance 'p' past C/C++ whitespace, including both kinds of comments
void skipCWhitespace(char const *&p);

// [a-zA-Z_] (start of a C identifier)
bool isCIdentifierBegin(char c);
// [a-zA-Z_0-9] (rest of a C identifier)
bool isCIdentifierChar(char c);

// read the C identifier starting at 'p': [a-zA-Z_][a-zA-Z0-9_]*
string readIdentifier(char const *&p);

// call 'readIdentifier', and throw XParse unless the result is
// equal to 'expected'
void expectIdentifier(char const *&p, char const *expected);

// read the quoted string starting at 'p'; must begin and end with
// the double-quote character; this will decode the escape sequences
// and strip the quotes
string readQuotedString(char const *&p);

// Same as above except the string doesn't start with '"' and may end
// with either '"' or a null character.
string unescapeCString(char const *&p);
string unescapeCString(const string &s);

// Options for generating an escaped C string.
enum EscapeOptions {
    ESCAPEOPT_NONE     = 0,
    // If set, then any non-ASCII character is left there, otherwise
    // it's turned into an escape sequence.
    // The idea is that in many cases, we want to keep UTF-8 sequences
    // intact when printing a string.
    ESCAPEOPT_KEEP_NONASCII = 1 << 0,
    // If set, then hex escapes will use uppercase
    ESCAPEOPT_HEX_UPCASE   = 1 << 1,
    // If set, then use an octal escape for a character that can be
    // represented with 1 octal digit, i.e. \0 to \7
    // Otherwise, use e.g. \x02 (hex escape with at least 2 digits),
    // except for \0.
    ESCAPEOPT_1DIGIT_OCTAL = 1 << 2
};

// Output an escaped version of "unescaped" (of length "len"
// characters) to "out", using the given options.
// "endContext" is the character that would come right after the
// string, i.e. unescaped[len]
// It is needed because of the ambiguity between a hex or octal escape
// digit and the next character.
// For instance,
// "a\x13b"
// is not the same as
// "a\x13""b"
// In case of ambiguity, it uses a 3-digit octal escape.
void outputEscapedCString(
    ostream &out,
    const char *unescaped,
    size_t len,
    char endContext,
    EscapeOptions opts);

// Struct allowing to calle outputEscapedCString will streaming
struct EscapedCString {
    EscapedCString(
        const char *unescaped,
        size_t len,
        char endContext,
        EscapeOptions opts):
        unescaped(unescaped),
        len(len),
        endContext(endContext),
        opts(opts) {}
    const char *unescaped;
    size_t len;
    char endContext;
    EscapeOptions opts;
};

ostream &operator<<(ostream &out, const EscapedCString &str);

// Calls "outputEscapedCString" with ESCAPEOPT_1DIGIT_OCTAL
string escapeCString(const string &unescaped);
string escapeCString(const char *unescaped, size_t len);

// Calls "outputEscapedCString" with ESCAPEOPT_HEX_UPCASE.
// The difference in options with 'escapeCString' is a historical
// accident, kept for compatibility.
string encodeWithEscapes(char const *src, int len);

// Same as "encodeWithEscapes", but surrounds the result in double
// quotes
string quoted(rostring src);

// If 'c' can be escaped with a "standard" escape (i.e. \ + a single
// char), return that char, otherwise return 0.
char escapeCChar(char c);

// In arrayset-funcs.h I need a hook at a cast site.  Let's try this.
// To call it, one must explicitly pass a template argument.
template <class T>
inline T convertFromLong(long i)
{
  // default implementation tries to do the conversion w/o an
  // explicit cast; this will fail for enumerations, so I will
  // create specializations for them
  return i;
}


// Given that a parse error occurred at location 'p' of a buffer that
// begins at 'start', deduce the line and column by counting newlines
// between them, and prepend that information plus 'fname' to the
// error message in 'x'.
void prependLineCol(XParse &x, char const *fname,
                    char const *start, char const *p);

void get_c_identifiers(istream &in,
                       set<string> &id_set);


#endif /* PARSE_HPP */
