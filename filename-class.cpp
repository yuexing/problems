// filename-class.cpp
// code for filename-class-impl.hpp
// (c) 2008-2014 Coverity, Inc. All rights reserved worldwide.

#include "filename-class-impl.hpp"               // this module
#include "file-operations.hpp"                   // also this module

#include "path.hpp"                              // path
#include "efstream.hpp"                          // efstreambuf

#include "libs/char-enc/encoding.hpp"            // Encoding
#include "libs/char-enc/console-encode.hpp"      // ConsoleEncode::conv_wide_to_multi_byte_utf8_string
#include "libs/cov-alloca.hpp"                   // alloca
#include "libs/containers/map.hpp"               // map
#include "libs/containers/set.hpp"               // set
#include "libs/containers/pair.hpp"              // pair
#include "libs/containers/hash-string.hpp"       // cov_hash_str
#include "libs/debug/output.hpp"                 // GDOUT
#include "libs/exceptions/sup-assert.hpp"        // sup_assert
#include "libs/exceptions/assert.hpp"            // ostr_assert, xassert
#include "libs/exceptions/exceptions.hpp"        // CAUTIOUS_RELAY
#include "libs/exceptions/user-error.hpp"        // throw_XUserError
#include "libs/test-util/check-comparator.hpp"   // check_comparator
#include "libs/file/stdio-file-utils.hpp"        // xfputc, xfputs_xmlEscape
#include "libs/exceptions/xsystem.hpp"           // throw_XSystem
#include "libs/refct-ptr.hpp"                    // refct_ptr
#include "libs/scoped-ptr.hpp"                   // scoped_array
#include "libs/system/is_windows.hpp"            // is_windows
#include "libs/system/cov-windows.hpp"           // GetLongPathName, GetCurrentDirectory
#include "libs/text/diff.hpp"                    // assert_same_string
#include "libs/text/iostream.hpp"                // ostream
#include "libs/text/parse.hpp"                   // quoted
#include "libs/text/sstream.hpp"                 // ostringstream
#include "libs/text/stream-utils.hpp"            // copy_stream
#include "libs/text/string.hpp"                  // string
#include "libs/text/string-utils.hpp"            // lc
#include "libs/text/stringb.hpp"                 // stringb
#include "libs/text/strtokp.hpp"                 // StrtokParse
#include "libs/tutils.hpp"                       // compare
#include "libs/debug/flags.hpp"                  // debug::filename

#include <sys/stat.h>                            // stat
#include <unistd.h>                              // getcwd
#include <dirent.h>                              // opendir, readdir
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>

OPEN_NAMESPACE(FilenameNS)

#ifdef __MC_SOLARIS__
#define readdir readdir64
#define dirent  dirent64
#endif

// Filename node table
//
// The first element is always the "super-root" of the name tree.
// The user-visible "roots" are actually  children of this node.
// It is initialized on demand.  Once created, it is never deallocated.
std::vector<FilenameNode *> *filenameNodes = NULL;

// Free list of available indices in filename node table.
//
// Use these before growing the table.  This avoids having
// to use indirection or do messy updates of references, and
// limits the size of the table <= peak number of indices.
std::vector<FilenameNodeIndex> *freeFilenameNodeIndexes = NULL;

const char *FQNFileSystem = "#FQN:";
const char *getFQNFileSystem()
{
    return FQNFileSystem;
}

// Filename class uses 31 bits to store indices.
// Check that we don't exceed that representation.
#define MAX_SAFE_FILENAMENODE_INDEX ((1U << 31) - 1)

// ------------------------- Filename ---------------------------
Filename::Filename(FilenameNode *node, SystemStringEncoding encoding)
  : node_idx(node->index),
    encoding(encoding)
{
    // We do not bump the refct; that is the caller's responsibility.
}

Filename::~Filename()
{
    getNode()->decRefct();
}

static void assertMatchingEncoding(
    SystemStringEncoding enc1,
    SystemStringEncoding enc2)
{
    string_assert(enc1 == enc2, "Mismatching filename encodings");
}

// Moved to /libs/char-enc/console-encode.cpp
// static string conv_wide_to_multi_byte_utf8_string(const wchar_t *f)

static bool isLetter(char c)
{
    // not using 'isalpha' to avoid locale dependence (but still
    // assume ASCII)
    return ('A' <= c && c <= 'Z') ||
           ('a' <= c && c <= 'z');
}

static inline bool isWinSeparator(char c)
{
    return c == '/' || c == '\\';
}
static inline bool isUnixSeparator(char c)
{
    return c == '/';
}

static bool (*getSeparatorCheck(Filename::FilenameInterp interp))(char) {
    return (interp == Filename::FI_WINDOWS) ?
        isWinSeparator : isUnixSeparator;
}

// Compares null-terminated s1 with [s2;s2+len)
static int strcmp_len2(const char *s1,
                const char *s2,
                int length) {
    if(int cmp = strncmp(s1, s2, length)) {
        return cmp;
    }
    return s1[length] ? 1 : 0;
}

#if defined(__MC_MINGW__)
/* Only attempts to convert file operation error codes. */
int getErrnoFromWindowsError(uint32 errcode) {
    /* Refs: http://www.barricane.com/c-error-codes-include-errno
             http://msdn.microsoft.com/en-us/library/windows/desktop/ms681381%28v=vs.85%29.aspx
     */
    switch (errcode) {
        case ERROR_SUCCESS:
            return 0;

        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_INVALID_HANDLE:
        case ERROR_NOT_READY:
        case ERROR_INVALID_DRIVE:
            // ERROR_INVALID_DRIVE used to map to ENODEV, but I think
            // that "ENOENT" is safer (since "file not found" is often
            // a less serious error)
            return ENOENT;

        case ERROR_NO_MORE_FILES:
        case ERROR_TOO_MANY_OPEN_FILES:
            return EMFILE;

        case ERROR_WRITE_PROTECT:
        case ERROR_INVALID_ACCESS:
        case ERROR_ACCESS_DENIED:
            return EACCES;

        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
        case ERROR_USER_MAPPED_FILE:
            // smcpeak 2014-04-05: The first two were previously
            // mapped to EACCES, but they are concurrency errors, not
            // permission errors.  EBUSY is what should be used for
            // the former.  I added ERROR_USER_MAPPED_FILE while
            // working on bug 62701, although my ultimate fix for that
            // bug does not go through this code.
            return EBUSY;

        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return ENOMEM;

        case ERROR_INVALID_DATA:
            return EINVAL;

        case ERROR_FILE_EXISTS:
            return EEXIST;

        case ERROR_OPEN_FAILED:
            return EIO;

        case ERROR_FILENAME_EXCED_RANGE:
            // This may be redone by map_last_OS_error_to_errno()
            return ENAMETOOLONG;

        case ERROR_DISK_FULL:
            return ENOSPC;

    }
    return -1;
}
#endif /* __MC_MINGW__ */

// Parse a filesystem (drive or UNC)
// After this function, 'name' points to the file name part with the
// filesystem remove, and beginning_of_fs/end_of_fs indicate the range
// of characters making the file system.
// allocated_fs may be an allocated buffer containing the characters
// of the filesystem; it's pointed to by beginning_of_fs/end_of_fs; it
// should not be looked at directly.
static void parseFileSystem(const char *&name,
                            scoped_array<char> &allocated_fs,
                            const char *&beginning_of_fs,
                            const char *&end_of_fs) {
    end_of_fs = 0;
    beginning_of_fs = name;
    // This hack is used to represent FQNs as if they were files in a special file system.
    if(begins_with(name, FQNFileSystem)) {
        name += strlen(FQNFileSystem);
    } else if(isLetter(name[0]) && name[1] == ':') {
        // Drive letter
        name += 2;
    } else if(isWinSeparator(name[0]) && name[1] == name[0]) {
        // UNC
        const char *start = name;
        name +=2;

        // Special case: \\?\ is not really UNC. Skip it, if present.
        // cygpath produces such strings
        // Also, turn '\\?\UNC\' into '\\'
        // See toWindowsStringWithLongNamePrefix
        if(*name == '?' && isWinSeparator(name[1])) {
            name += 2;
            if(begins_withi(name, "UNC")
               && isWinSeparator(name[3])) {
                // Skip to the beginning of the host name
                name += 4;
                // Pretend we started at the 'C' of 'UNC', which is 2
                // chars before the host name, where the first '\'
                // would have been.
                // This triggers the "*start != '\\'" check below
                start = name - 2;
            } else {
                parseFileSystem(
                    name,
                    allocated_fs,
                    beginning_of_fs,
                    end_of_fs);
                return;
            }
        }

        for(; *name && !isWinSeparator(*name); ++name) {}
        // If specified '//' instead of '\\', allocate a string with
        // '//' replaced with '\\'
        if(*start != '\\') {
            int len = name - start;
            allocated_fs.reset(new char[len]);
            char *newfs = allocated_fs.get();
            newfs[0] = newfs[1] = '\\';
            memcpy(newfs + 2, start + 2, len - 2);
            beginning_of_fs = newfs;
            end_of_fs = newfs + len;
        }
    } // else leave "name" unchanged
    if(!end_of_fs) {
        end_of_fs = name;
    }
}

// Use the heuristic parsing specified for the Filename constructor to
// set 'fs' and 'absolute', moving 'name' past any characters that
// have been interpreted.
static void parseNameStart
(char const *&name,
 scoped_array<char> &allocated_fs,
 const char *&beginning_of_fs,
 char const *&end_of_fs,
 bool &absolute,
 Filename::FilenameInterp interp)
{
    // filesystem name
    if (interp == Filename::FI_PORTABLE
        ||
        interp == Filename::FI_WINDOWS
        ) {
        parseFileSystem
            (name,
             allocated_fs,
             beginning_of_fs,
             end_of_fs);
    } else {
        beginning_of_fs = end_of_fs = name;
    }

    // absolute?
    bool (*isSeparator)(char) = getSeparatorCheck(interp);
    absolute = isSeparator(*name);
    if (absolute) {
      name++;
      // Skip all separators
      while(isSeparator(*name))
          name++;
    }
}

static bool indicates_win_style(Filename::FilenameInterp fn) {
    return fn == Filename::FI_WINDOWS;
}

class FilenameBuilder {
public:
    static bool appendNames
    (const char *name,
     int length,
     bool isStart,
     bool isAbsolute,
     void *arg) {
        if(isStart)
            return true;
        Filename &f = *static_cast<Filename *>(arg);
        f.appendSingleNameWithLen(name, length);
        return true;
    }
    static bool initFile
    (const char *name,
     int length,
     bool isStart,
     bool isAbsolute,
     void *arg) {
        Filename &f = *static_cast<Filename *>(arg);
        if(isStart) {
            f.node_idx = getRootNode(name, length, isAbsolute)->index;
        } else {
            f.appendSingleNameWithLen(name, length);
        }
        return true;
    }
};


void Filename::init(const char *name,
                    FilenameInterp fn) {
    parseFilename(name,
                  FilenameBuilder::initFile,
                  (void *)this,
                  fn);
}

Filename::Filename(char const *name, FilenameInterp fn, SystemStringEncoding encoding)
  : node_idx(0),  // will be set in init
    encoding(encoding)
{
    init(name, fn);
}

Filename::Filename(rostring name, FilenameInterp fn, SystemStringEncoding encoding)
  : node_idx(0),  // will be set in init
    encoding(encoding)
{
    init(name.c_str(), fn);
}

Filename::Filename(const path &p, FromBoostPath dummy, SystemStringEncoding encoding)
  : node_idx(0),  // will be set in init
    encoding(encoding)
{
    sup_assert(!p.empty());
    init(p.string().c_str(), FI_PORTABLE);
}


Filename::Filename(Filename const &obj)
  : node_idx(obj.node_idx),
    encoding(obj.encoding)
{
    getNode()->incRefct();
}


Filename::Filename(SystemStringEncoding encoding)
    : node_idx(getRootNode("", 0, false /*absolute*/)->index), // bumps refct
      encoding(encoding)
{}


FilenameNode * Filename::getNode() const {
    return FilenameNode::get(node_idx);
}


Filename& Filename::operator= (Filename const &obj)
{
    if (node_idx != obj.node_idx) {
        getNode()->decRefct();
        node_idx = obj.node_idx;
        getNode()->incRefct();
    }
    encoding = obj.encoding;
    return *this;
}

bool Filename::isValidForOS(FilenameInterp fn) const
{
    if (fn == FI_WINDOWS) {
        unsigned pathlen = 0;

        // Windows naming conventions from MSDN
        // http://msdn.microsoft.com/en-us/library/aa365247.aspx
        for (Filename tmp = *this; tmp.hasNames(); tmp = tmp.parent()) {
            const char *component = tmp.finalName();
            const unsigned complen = strlen(component);

            // compute path length
            pathlen += complen + /* path separator */ 1;

            // look for illegal characters
            for (const char *tmp = component; *tmp; ++tmp) {
                if (*tmp <= 31)
                    return false;
                if (strchr("<>:\"/\\|?*", *tmp))
                    return false;
            }

            // look for reserved file names
            if (strncasecmp(component, "CON", 3)==0
                || strncasecmp(component, "PRN", 3)==0
                || strncasecmp(component, "AUX", 3)==0
                || strncasecmp(component, "NUL", 3)==0) {
                // both NUL and NUL.txt are disallowed
                if (complen==3 || component[3]=='.')
                    return false;
            }
            if (strncasecmp(component, "COM", 3)==0
                || strncasecmp(component, "LPT", 3)==0) {
                // COM1 is bad but COM0 is okay...
                if (complen > 3 && isdigit(component[3]) && component[3]!='0') {
                    if (complen==4 || component[4]=='.')
                        return false;
                }
            }

            // can't end with a period, except obviously for ".."
            if (component[complen-1]=='.'
                && strcmp(component, ".."))
                return false;
        }

        // fenceposts
        if (pathlen)
            pathlen--;

        // I'd like to check path length (BZ 26927) but MAX_PATH is not
        // defined on all platforms, so just check that I'm using the right
        // number here.
#ifdef _WIN32
        STATIC_ASSERT(MAX_PATH==260);
#endif
        return pathlen <= (260 - /* C:\ */ 3 - /* null terminator */ 1);
    } else {
        return true;
    }
}

void Filename::throwIfInvalidForHost() const
{
    if (!isValidForHost())
        throw_XUserError(stringb("Invalid file name: " << *this));
}

STATICDEF Filename Filename::getRoot(char const *fs, bool absolute, SystemStringEncoding encoding)
{
    return Filename(getRootNode(fs, strlen(fs), absolute), encoding);
}


Filename Filename::withAppendedSingleName(char const *name) const
{
    Filename ret(*this);
    ret.appendSingleName(name);
    return ret;
}


Filename Filename::withAppendedNames(Filename const &names) const
{
    Filename ret(*this);
    ret.appendNames(names);
    return ret;
}


Filename Filename::withAppendedNames(char const *names) const
{
    Filename ret(*this);
    ret.appendNames(names);
    return ret;
}

Filename Filename::withAppendedNames(const string &names) const
{
    return withAppendedNames(names.c_str());
}


void Filename::appendSingleNameWithLen(char const *name, int length)
{
    cond_assert(length != 0);

    FilenameNode *child = getNode()->getOrCreateChild(name, length);

    // move the 'node' pointer
    getNode()->decRefct();
    node_idx = child->index;

    // no need to inc 'child' since 'getOrCreateChild' did that
}


Filename& Filename::appendSingleName(char const *name)
{
    appendSingleNameWithLen(name, strlen(name));
    return *this;
}

Filename& Filename::appendSingleName(rostring name)
{
    appendSingleNameWithLen(name.c_str(), name.size());
    return *this;
}


void Filename::appendNamesFromNode(FilenameNode *node)
{
    if (!node->isRoot()) {
        // append parent names first
        appendNamesFromNode(node->parent);
        
        // then child
        appendSingleName(node->name);
    }
    else {
        // Ignore root
    }
}


Filename& Filename::appendNames(Filename const &names)
{
    assertMatchingEncoding(this->encoding, names.encoding);
    appendNamesFromNode(names.getNode());
    return *this;
}


// Return pointer to first occurrence of a separator character, or
// to the end of the string if none appears in 'name'.
static char const *findSeparator(char const *name,
                                 Filename::FilenameInterp interp)
{
    bool (*isSeparator)(char) = getSeparatorCheck(interp);
    char c;
    while ((c = *name) != 0 && !isSeparator(c)) {
        name++;
    }
    return name;
}

// Skip any amount of leading "./" from "start" (doesn't need to be
// followed by '/' if at the end)
static void skipDots(char const *&start,
                     Filename::FilenameInterp interp) {
    bool (*isSeparator)(char) = getSeparatorCheck(interp);
    while (*start == '.') {
        char c = start[1];
        if(isSeparator(c)) {
            start += 2;
            while(isSeparator(*start))
                ++start;
        } else if(!c) {
            ++start;
            return;
        } else {
            return;
        }
    }
}

// Find the beginning and end of the component starting at "start"
// (which will be updated to point at the beginning), skipping any
// './'.
// end_of_component will be the end of the component name (on Windows
// we may need to remove some trailing chars. Specifically, any series
// of trailing '.' and ' ' will be removed),
// beginning_of_next_component is the beginning of the next component,
// with separators stripped, suitable to pass as "start" to this same
// function (dots are not stripped in that case though)
//
// If there is no next component (e.g. only "./"), end_of_component == start
static void findNextComponent
(char const *&start,
 char const *&end_of_component,
 char const *&beginning_of_next_component,
 Filename::FilenameInterp interp) {
    bool use_win_style = indicates_win_style(interp);
    skipDots(start, interp);
    if(!*start) {
        end_of_component = beginning_of_next_component = start;
        return;
    }
    end_of_component = findSeparator(start, interp);
    beginning_of_next_component = end_of_component;
    if(use_win_style) {
        char c;
        // Remove all trailing '.' and ' ', they're ignored.
        while(end_of_component > start
              &&
              ((c = end_of_component[-1]) == '.' || c == ' ')) {
            --end_of_component;
        }
        // If we removed all chars, undo the change.
        // The only valid case on Windows should be ".."
        if(end_of_component == start) {
            end_of_component = beginning_of_next_component;
        }
    }
    bool (*isSeparator)(char) = getSeparatorCheck(interp);
    while(isSeparator(*beginning_of_next_component)) {
        ++beginning_of_next_component;
    }
}

bool Filename::
parseFilename(const char *name,
              bool (*fn)(const char *name,
                         int length,
                         bool isStart,
                         bool isAbsolute,
                         void *arg),
              void *arg,
              FilenameInterp interp) {
    GDOUT(filename, "parseFilename: " << name);
    // root designator
    char const *end_of_fs;
    char const *beginning_of_fs = name;
    scoped_array<char> allocated_fs;
    bool absolute;
    parseNameStart
        (name,
         allocated_fs,
         beginning_of_fs,
         end_of_fs,
         absolute,
         interp);

    if(!fn(beginning_of_fs,
           end_of_fs - beginning_of_fs,
           /*isStart*/true,
           absolute,
           arg))
        return false;
    allocated_fs.reset();

    // process each name component
    for (;;) {

        char const *end;
        char const *next;

        findNextComponent(name,
                          end,
                          next,
                          interp);
                          
        if (name == end) {
            // no more name components
            return true;
        }

        // name before separator
        if(!fn(name,
               end-name,
               /*isStart*/false,
               /*isAbsolute*/false,
               arg))
            return false;
        name = next;
    }
}

namespace {
    struct get_final_name_t {
        const char *start;
        int length;
        static bool callback
        (const char *name,
         int length,
         bool isStart,
         bool isAbsolute,
         void *arg) {
            if(isStart)
                return true;
            get_final_name_t &s =
                *static_cast<get_final_name_t *>(arg);
            s.start = name;
            s.length = length;
            return true;
        }
    };
}


static void getFinalName
(const char *&start,
 int &length,
 Filename::FilenameInterp interp) {
    struct get_final_name_t s;
    s.start = 0;
    s.length = 0;
    Filename::parseFilename(start,
                  get_final_name_t::callback,
                  &s,
                  interp);
    start = s.start;
    length = s.length;
}

Filename& Filename::appendNames(char const *names)
{
    return appendNames(names, FI_MAGIC);
}

Filename& Filename::appendNames(rostring names)
{
    return appendNames(names.c_str(), FI_MAGIC);
}

Filename& Filename::appendNames(rostring names,
                                FilenameInterp fn)
{
    return appendNames(names.c_str(), fn);
}

Filename &Filename::appendNames
(char const *names,
 FilenameInterp fn) {
    parseFilename
        (names,
         FilenameBuilder::appendNames,
         (void *)this,
         fn);

    return *this;
}

static bool same_drive_letter(const char *d1, const char *d2) {
    return tolower(d1[0]) == tolower(d2[0]);
}

// Unfortunately we need to maintain two separate caches, as system
// default and UTF-8 encoded file names should not be compared or used
// together in any way.
static map<Filename, pair<Filename, bool> > symLinkCache[NUM_SYSTEM_STRING_ENCODINGS];

// Index = boolean "is short"
// For some reason, we only allows this for system-encoded files, so
// no index on encoding.
static map<Filename, pair<Filename, bool> > longshortCache[2];

// This is a map from file name to case-normalized final name (string
// is UTF-8 or system-default encoded)
static map<Filename, pair<string, bool> > normalizedCaseCache[NUM_SYSTEM_STRING_ENCODINGS];

void Filename::clearSymlinkCache() {
    for(int i = 0; i < ARRAY_SIZE(symLinkCache); ++i) {
        symLinkCache[i].clear();
    }
}

bool Filename::resolveLongOrShortName
(bool shortName) {
#if !defined(__MC_MINGW__)
    return false;
#else
    string_assert(
        encoding != SSE_UTF8,
        "resolveLongOrShortName can only be used with system default encoded file names");
    pair<map<Filename, pair<Filename, bool> >::iterator, bool> p;
    if(!hasNames()) {
        // No name to make long or short
        return false;
    }
    if(isAbsolute()) {
        p = longshortCache[shortName].insert
            (make_pair(*this, make_pair(Filename(this->encoding), false)));
    }
    if(!isAbsolute() || p.second) {
        DWORD WINAPI (*getName)(LPCTSTR, LPTSTR, DWORD)
            = shortName ?
            GetShortPathNameA :
            GetLongPathNameA;
        // We know through bitter experience that Windows filenames
        // can't exceed 256 chars (+ 3 for drive letter, + 1 for
        // terminating NUL)
        // This used to call once with NULL / 0 size, but I'd rather
        // aboid doing 2 syscalls when I can only do 1
        static const int max_path_size = 260;
        // This is easily small enough to fit on the stack
        char buf[max_path_size];
        scoped_array<char> toDelete;
        const char *longName = 0;
        string asStr = toString();
        int size = getName(asStr.c_str(), buf, max_path_size);
        // size should not be equal to max_path_size (since in the
        // success case, it doesn't count the terminating NUL)
        if(size > max_path_size) {
            // This should not happen since our buffer's size is the
            // max allowed, but I like to be ready for everything.
            toDelete.reset(new char[size]);
            int rv = getName(asStr.c_str(), buf, size);
            // This condition should be true
            if(rv) {
                longName = toDelete;
            }
        } else if(size) {
            longName = buf;
        } else {
            // Failure
            longName = 0;
        }
        bool rv = false;
        if(longName) {
            // Only get the final name, to avoid uselessly
            // rebuilding a path.
            int length;
            getFinalName(longName, length, Filename::FI_HOST);
            // We know this is not a root, so there should be
            // a final name.
            string_assert(longName != 0, "There should be a final name!");
            if(strcmp_len2(finalName(),
                           longName, length)) {
                *this = parent();
                appendSingleNameWithLen(longName, length);
                if(isAbsolute()) {
                    // Means "p" is valid
                    p.first->second.first = *this;
                    p.first->second.second = true;
                }
                rv = true;
            } // else path is unchanged
        }
        return rv;
    } else {
        // Means "p" is valid
        if(p.first->second.second) {
            // Means we need to do the transformation
            *this = p.first->second.first;
        } else {
            // Keep "this" unchanged
        }
        return p.first->second.second;
    }
#endif
}

bool Filename::resolveLongName() {
    return resolveLongOrShortName(/*shortName*/false);
}

bool Filename::resolveShortName() {
    return resolveLongOrShortName(/*shortName*/true);
}

bool Filename::resolveSymlink() {
#if !defined(__MC_MINGW__)
    // Map from absolute filename to a bool indicating whether this
    // was a symlink, and if it was, what the symlink resolved to
    // If not absolute, don't cache, as the result depends on the
    // current directory (and the main use of it, the "normalized"
    // series of functions, only calls this on absolute paths)
    pair<map<Filename, pair<Filename, bool> >::iterator, bool> p;
    if(isAbsolute()) {
        p = symLinkCache[encoding].insert(make_pair(*this, make_pair(Filename(this->encoding), false)));
    }
    if(!isAbsolute() || p.second) {
        char buf[PATH_MAX];
        int rv = readlink(toSystemDefaultString().c_str(), buf, PATH_MAX-1);
        if(rv > 0) {
            buf[rv] = '\0';
            // Symlink is relative the parent, which right now is
            // "completed"
            if(buf[0] == '/') {
                // Absolute, use the target
                if (isUTF8Encoded()) {
                    string utf8_buf = Encoding::defaultEncodingToUTF8(buf);
                    *this = Filename(utf8_buf, FI_HOST, this->encoding);
                } else {
                    *this = Filename(buf, FI_HOST, this->encoding);
                }
            } else {
                // Relative, prefix with parent
                *this = parent();
                if (isUTF8Encoded()) {
                    string utf8_buf = Encoding::defaultEncodingToUTF8(buf);
                    appendNames(utf8_buf, FI_HOST);
                } else {
                    appendNames(buf, FI_HOST);
                }
            }
            if(isAbsolute()) {
                // Means "p" is valid
                p.first->second.first = *this;
                p.first->second.second = true;
            }
            return true;
        } else {
            // Keep "false" in the map
            // Also keep the default-constructed Filename which we
            // don't care about
            return false;
        }
    } else {
        // Means "p" is valid
        if(p.first->second.second) {
            // Means this is a symlink, and we need to do the transformation
            *this = p.first->second.first;
        } else {
            // Keep "this" unchanged
        }
        return p.first->second.second;
    }
#else
    return false;
#endif
}

bool Filename::resolveAllSymlinks() {
    int loopCount = 0;
    // Ideally we'd check sysconf(SYMLOOP_MAX), but I don't think it's
    // worth the trouble.
    static const int max_symlinks = 32;
    // Remember current file so we can report errors
    Filename bak = *this;
    bool rv = false;
    while(resolveSymlink()) {
        rv = true;
        if(++loopCount > max_symlinks) {
            throw_XUserError
                (stringb("Trying to resolve "
                         << bak << ": too many levels of symlinks"));
        }
    }
    return rv;
}


// flags -> completed filename -> (normalized filename, hasChanged)
// Unfortunately we need seperate caches since UTF-8 and system default
// file names cannot be compared or used together in any way.
static map<Filename::NormalizeFlags, map<Filename, pair<Filename, bool> > >
allNormalizeCaches[NUM_SYSTEM_STRING_ENCODINGS];

Filename Filename::normalizedFile
(const char *f,
 FilenameInterp interp,
 NormalizeFlags flags,
 const Filename *base,
 SystemStringEncoding encoding) {
    if (base) {
        assertMatchingEncoding(base->encoding, encoding);
    }

    // Do not build a Filename of which we might potentially discard
    // parts, if at all possible.
    // Instead, parse and normalize at the same time.
    bool absolute;
    char const *orig_f = f;
    char const *end_of_fs;
    scoped_array<char> allocated_fs;
    const char *beginning_of_fs;
    parseNameStart(f,
                   allocated_fs,
                   beginning_of_fs,
                   end_of_fs,
                   absolute,
                   interp);

    Filename workPath(encoding);

    if(absolute) {
        string fs(beginning_of_fs, end_of_fs - beginning_of_fs);
        allocated_fs.reset();

        if(fs.empty()) {
            // Possibly get filesystem from the base
            // Do we need to add a filesystem?
            const Filename &b = base ? *base : initialPath(encoding);
            if(b.hasFilesystem()) {
                // Yes
                // We may need to lowercase it if we got it from initialPath()
                fs = b.getFilesystem();
                if(!base) {
                    if(flags & NF_LOWERCASE_ALWAYS) {
                        makeLowerCase(fs);
                    } else if(flags & NF_NORMALIZE_CASE) {
                        makeUpperCase(fs);
                    }
                }
            }
        } else {
            if(flags & NF_LOWERCASE_ALWAYS) {
                makeLowerCase(fs);
            } else if(flags & NF_NORMALIZE_CASE) {
                makeUpperCase(fs);
            }
        }
        workPath = getNamedRoot(fs.c_str(), encoding);
    } else {
        // Special case, relative with drive letter.
        if(end_of_fs > beginning_of_fs) {
            allocated_fs.reset();
            // Use algo in normalized(). This is rare enough that I don't
            // care about performance at this point.
            return Filename(orig_f, interp, encoding).normalized(flags, base);
        }

        bool hasChanged;
        workPath = base ?
            *base :
            initialPath(encoding).normalizedWhenComplete(flags, hasChanged);
    }

    // Append names one at a time, while normalizing.
    // We don't want to create Filenames that we aren't going to use.
    for(;;) {
        const char *end;
        const char *next;
        findNextComponent(f,
                          end,
                          next,
                          interp);
                
        if (f == end) {
            // no more name components
            return workPath;
        }
        workPath.normalizedAppendSingleNameWithLen
            (flags,
             f,
             end - f,
             /*withAppended*/0);
        f = next;
    }
}

Filename Filename::normalized
(NormalizeFlags flags,
 const Filename *base) const {
    bool hasChanged;
    return normalized(flags, base, hasChanged);
}

OPEN_ANONYMOUS_NAMESPACE;

// Wrapper for a byte buffer that contains either single byte (system
// default) or wide-char data.
// Only contains wide-char data on Windows; on Unix, always contains a
// system default string.
struct EncodedFileName {
    explicit EncodedFileName(const char *data):
        isWideChar(false),
        adata(data) {
    }
    explicit EncodedFileName(const wchar_t *data):
        isWideChar(true),
        wdata(data) {
    }
    bool isWideChar;
    union {
        const wchar_t *wdata;
        const char *adata;
    };
};

// Macro that return a temporary object with the right data for the
// Filename object.
// It's a macro because the temporary object keeps references to temporary
// byte buffers (c_str())
#ifdef __MC_MINGW__
#define ENCODED_FILE_NAME(file)                                         \
    ((file).isUTF8Encoded() ?                                           \
     EncodedFileName(toWindowsStringWithLongNamePrefixUnicode(file).c_str()): \
     EncodedFileName((file).toString().c_str()))
#else
#define ENCODED_FILE_NAME(file)                             \
    EncodedFileName((file).toSystemDefaultString().c_str())
#endif

CLOSE_ANONYMOUS_NAMESPACE;

// Call a Windows file API on the given file. Uses the W (unicode) variant if
// UTF-8 encoded, otherwise the A (ASCII) variant.
#define CALL_WIN_API(api, file, ...)                                    \
    ((file).isUTF8Encoded() ?                                           \
     api##W(toWindowsStringWithLongNamePrefixUnicode(file).c_str(), ##__VA_ARGS__) : \
     api##A((file).toString().c_str(), ##__VA_ARGS__))

// Same as above, except the argument is an EncodedFileName.
#define CALL_WIN_API_STR(api, file, ...)        \
    ((file).isWideChar ?                        \
     api##W((file).wdata, ##__VA_ARGS__) :      \
     api##A((file).adata, ##__VA_ARGS__))

Filename Filename::normalized
(NormalizeFlags flags,
 const Filename *base,
 bool &hasChanged) const {
    hasChanged = false;

    if (base) {
        assertMatchingEncoding(base->encoding, this->encoding);
    }
    string_assert(!base
                  ||
                  (base->isAbsolute() &&
                   (!is_windows || base->hasFilesystem())),
                  "base should be absolute and, on Windows, include a filesystem");

    Filename workPath(*this);

    // First complete if necessary. The cache can only work on
    // completed filenames (otherwise we'd need to check "base", too)
    if(!workPath.isAbsolute()) {
        const Filename &b = base ? *base : initialPath(this->encoding);
        if(workPath.hasFilesystem()
           &&
           !same_drive_letter(workPath.getFilesystem(), b.getFilesystem())) {
            // Windows degenerate case: "d:foo"
            // Where "d:" is not the current drive
#ifndef __MC_MINGW__
            // This was using boost::filesystem::system_complete, which
            // appends to getcwd() on POSIX-like systems.  Use the provided
            // base instead.  Neither way makes much sense, but at least using
            // the provided base conforms better to the interface we're
            // presenting.
            //
            // Reinterpret the filename as FI_UNIX so that the drive letter is
            // appended.  That's what boost::filesystem::system_complete did.
            workPath = b / Filename(workPath.toString(), FI_UNIX, this->encoding);
#else
            if (isUTF8Encoded()) {
                DWORD size = MAX_PATH;
                GrowArray<wchar_t> path(size);
                DWORD len = GetFullPathNameW(toWindowsStringWithLongNamePrefixUnicode(*this).c_str(),
                                            size, path.getArrayNC(), NULL);

                if (len == 0) {
                    throw_XSystem("GetFullPathNameW", workPath.toString());
                }
                if (len > size) {
                    size = len;
                    path.ensureAtLeast(size);
                    len = GetFullPathNameW(toWindowsStringWithLongNamePrefixUnicode(*this).c_str(),
                                           size, path.getArrayNC(), NULL);
                    if (len == 0) {
                        throw_XSystem("GetFullPathNameW (2)", workPath.toString());
                    }
                }

                workPath = Filename(
                    ConsoleEncode::conv_wide_to_multi_byte_utf8_string(path.getArray()),
                    FI_WINDOWS,
                    this->encoding);
            } else {
                // Same as above, with char instead of wchar_t.
                // The CALL_WIN_API macros don't work because there
                // are 2 char buffer arguments.
                DWORD size = MAX_PATH;
                GrowArray<char> path(size);
                DWORD len = GetFullPathNameA(workPath.toString().c_str(),
                                            size, path.getArrayNC(), NULL);

                if (len == 0) {
                    throw_XSystem("GetFullPathNameA", workPath.toString());
                }
                if (len > size) {
                    size = len;
                    path.ensureAtLeast(size);
                    len = GetFullPathNameA(workPath.toString().c_str(),
                                          size, path.getArrayNC(), NULL);
                    if (len == 0) {
                        throw_XSystem("GetFullPathNameA (2)", workPath.toString());
                    }
                }

                workPath = Filename(path.getArray(), FI_WINDOWS, this->encoding);
            }
#endif
        } else {
            // Don't build a Filename when we don't need to.
            // Use normalizedAppendSingleNameWithLen

            // We need to reverse the order of names
            vector<const char *> nodesNames;
            nodesNames.reserve(numNames());
            FilenameNode *n = getNode();
            while(!n->isRoot()) {
                nodesNames.push_back(n->name);
                n = n->parent;
            }
            Filename rv = b;
            if(!base) {
                // If a base is not given, normalize the initial path
                rv = rv.normalizedWhenComplete(flags, hasChanged);
            }
            while(!nodesNames.empty()) {
                const char *n = nodesNames.back();
                int length = strlen(n);
                nodesNames.pop_back();
                rv.normalizedAppendSingleNameWithLen
                    (flags, n, length,
                     /*withAppended*/0);
            }
            hasChanged = true;
            return rv;
        }
        // Completing is a change
        hasChanged = true;
    } else if(!workPath.hasFilesystem()) {
        // On Windows, we may need to add the drive letter, even if
        // absolute.
        // Add the base's filesystem, if any.
        const Filename &b = base ? *base : initialPath(this->encoding);
        if(b.hasFilesystem()) {
            string fs = b.getFilesystem();
            // We may need to lowercase it if we got it from initialPath()
            if(!base) {
                if(flags & NF_LOWERCASE_ALWAYS) {
                    makeLowerCase(fs);
                } else if(flags & NF_NORMALIZE_CASE) {
                    // When we normalize the case, the filesystem
                    // (drive letter / UNC host) is made upper case.
                    makeUpperCase(fs);
                }
            }
            workPath = getNamedRoot(fs.c_str(), this->encoding) / workPath;
            hasChanged = true;
        }
    }

    bool hasChanged2;
    Filename rv = workPath.normalizedWhenComplete(flags, hasChanged2);
    hasChanged = hasChanged || hasChanged2;
    return rv;
}

// this needs to be at file scope so that clearNormalizeCache can clear it,
// thus avoiding a use-after-free.
static map<Filename, pair<Filename, bool> > *prevCache = 0;

Filename Filename::normalizedWhenComplete
(NormalizeFlags flags,
 bool &hasChanged) const {
    hasChanged = false;
    static NormalizeFlags prevFlags = (NormalizeFlags)-1;
    static SystemStringEncoding prevEncoding = (SystemStringEncoding)-1;
    if(flags != prevFlags || encoding != prevEncoding || !prevCache) {
        // Flags or encoding should almost never change, so avoid a lookup
        // into the map in the usual case.
        prevFlags = flags;
        prevEncoding = encoding;
        prevCache = &allNormalizeCaches[encoding][prevFlags];
    }
    map<Filename, pair<Filename, bool> > &cache = *prevCache;

    string_assert(isAbsolute() &&
                  (!is_windows || hasFilesystem()),
                  "File in normalizedWhenComplete should be absolute and, "
                  "on Windows, include a filesystem");

    Filename workPath(*this);

    // Possibly lowercase the filesystem component
    if(workPath.hasFilesystem() &&
       (flags & (NF_LOWERCASE_ALWAYS | NF_NORMALIZE_CASE))) {
        const string fileSystem = workPath.getFilesystem();
        string fileSystemNorm =
            (flags & NF_LOWERCASE_ALWAYS) ? lowerCased(fileSystem) : upperCased(fileSystem);
        if(fileSystemNorm != fileSystem) {
            workPath = getNamedRoot(fileSystemNorm.c_str(), this->encoding) / workPath;
            hasChanged = true;
        }
    }
    if(workPath.isRoot()) {
        return workPath;
    }
    LET(p, cache.insert(make_pair(workPath, make_pair(Filename(this->encoding), hasChanged))));
    if(p.second) {
        // Normalize parent
        bool parentChanged;
        Filename parent = workPath.parent().normalizedWhenComplete
            (flags, parentChanged);
        if(parentChanged) {
            hasChanged = true;
        }
        // Look at final name
        // This is only valid as long as we don't modify workPath
        const char *finalName = workPath.finalName();
        int length = strlen(finalName);

        // workPath is "withAppended" only if parentChanged is false
        if(parent.normalizedAppendSingleNameWithLen
           (flags, finalName, length,
            parentChanged ? 0: &workPath)) {
            workPath = parent;
            hasChanged = true;
        } else {
            // This means the result is the current workPath
        }

        p.first->second.first = workPath;
        p.first->second.second = hasChanged;
    } else {
        hasChanged = p.first->second.second;
    }
    return p.first->second.first;
}

bool Filename::normalizedAppendSingleNameWithLen
(NormalizeFlags flags,
 char const *toAdd,
 int length,
 const Filename *withAppended) {
    GDOUT(filename,
          "normalizedAppendSingleNameWithLen: " << *this << ", " <<
          toAdd << ", " << length);
    bool hasChanged = false;
    if(!(flags & Filename::NF_PRESERVE_RELATIVE) && !strcmp_len2("..", toAdd, length)) {
        // A ".." will never remain there, we'll always have a change
        hasChanged = true;
        // If "leaf" is .., we need to resolve symlinks (unless we
        // already did that in the recursive normalize call above)
        if(!(flags & NF_RESOLVE_SYMLINKS)) {
            resolveAllSymlinks();
        }
        // ".." until past the root is the root, just use the parent
        // if we got to the root at this point.
        if(hasNames()) {
            // Remove the final name (it can't be ".." since the
            // parent is already normalized)
            *this = parent();
        } else {
            // Result is equal to current value of "this"
        }
    } else {
        // String to remember the new final name, if any.
        if(flags & NF_LOWERCASE_ALWAYS) {
            string newFinal = lowerCased(string(toAdd, length));
            if(!str_equal(toAdd, newFinal)) {
                appendSingleNameWithLen(newFinal.c_str(), newFinal.size());
                hasChanged = true;
            }
#ifdef __MC_MINGW__
        } else if(flags & NF_NORMALIZE_CASE) {
            string prevFinal(toAdd, length);
            Filename f = *this / prevFinal;
            LET(p, normalizedCaseCache[encoding].insert(
                    make_pair(f, make_pair(string(), false))));
            string &newFinal = p.first->second.first;
            bool &cachedHasChanged = p.first->second.second;
            if(!((flags & Filename::NF_PRESERVE_RELATIVE) && !strcmp_len2("..", toAdd, length)) && p.second) {
                WIN32_FIND_DATA findFileDataA;
                WIN32_FIND_DATAW findFileDataW;
                HANDLE h;
                if(isUTF8Encoded()) {
                    h = FindFirstFileW(toWindowsStringWithLongNamePrefixUnicode(f).c_str(), &findFileDataW);
                } else {
                    h = FindFirstFileA(f.toString().c_str(), &findFileDataA);
                }
                if(h == INVALID_HANDLE_VALUE) {
                    // Assume the file just doesn't exist.
                    // Windows has a lot of different errors for
                    // different badness of paths, it's a losing
                    // proposition to try and enumerate them all.
                    // Keep the original case.
                    cachedHasChanged = false;
                } else {
                    if(isUTF8Encoded()) {
                        newFinal = ConsoleEncode::conv_wide_to_multi_byte_utf8_string(findFileDataW.cFileName);
                    } else {
                        newFinal = findFileDataA.cFileName;
                    }
                    FindClose(h);
                    cachedHasChanged = !str_equal(prevFinal, newFinal);
                }
            }
            if(cachedHasChanged) {
                hasChanged = true;
                appendSingleNameWithLen(newFinal.c_str(), newFinal.size());
            }
#endif // __MC_MINGW__
        }
        // else no change
    }
    // At this point if "hasChanged" is false, we may still need to
    // add the final name.
    if(!hasChanged) {
        if(withAppended) {
            *this = *withAppended;
        } else {
            appendSingleNameWithLen(toAdd, length);
            // Mark hasChanged as "true"; we're only supposed to
            // return "true" if result is equal to "*withAppended"
            hasChanged = true;
        }
    }
    
    // Resolve symlinks / get long name after other normalizing, to
    // increase the chance of a cache hit in their caches
    if(flags & NF_RESOLVE_SYMLINKS) {
        if(resolveAllSymlinks()) {
            // Need to normalize the symlinks
            *this = normalizedWhenComplete(flags, hasChanged);
            // hasChanged needs to be set after call above (which
            // might set it to false)
            hasChanged = true;
        }
    }
    if(flags & (NF_LONGNAME | NF_SHORTNAME)) {
        // Remember whether there was a change so far
        bool prevHasChanged = hasChanged;
        if(resolveLongOrShortName(/*shortName*/!(flags & NF_LONGNAME))) {
            hasChanged = true;
            if(flags & NF_LOWERCASE_ALWAYS) {
                // We may need to lowercase the final name at this
                // point
                const char *f = finalName();
                string l = lowerCased(f);
                if(!str_equal(l, f)) {
                    // Optimization: if resolve + lowercase + previous
                    // normalization is no-op, and withAppended is
                    // given, return false
                    // Note that prevHasChanged can only be false if
                    // withAppended is non-0
                    if(!prevHasChanged &&
                       str_equal(l, withAppended->finalName())) {
                        // Leave "hasChanged" as false, and revert to
                        // "withAppended"
                        hasChanged = false;
                        *this = *withAppended;
                    } else {
                        // Replace final name with "l"
                        *this = parent();
                        appendSingleNameWithLen(l.c_str(), l.size());
                    }
                }
            }
        }
    }
    return hasChanged;
}

void Filename::clearNormalizeCache() {
    prevCache = NULL;
    for(int i = 0; i < ARRAY_SIZE(allNormalizeCaches); ++i) {
        allNormalizeCaches[i].clear();
    }
    for(int i = 0; i < ARRAY_SIZE(normalizedCaseCache); ++i) {
        normalizedCaseCache[i].clear();
    }
}

Filename Filename::completed(const Filename &base) {
    if(isAbsolute())
        return *this;
    return base / *this;
}

Filename Filename::currentPath(SystemStringEncoding encoding) {
#ifndef __MC_MINGW__
    char cwdBuffer[PATH_MAX];
    if (getcwd(cwdBuffer, PATH_MAX) == NULL) {
        throw_XSystem("getcwd");
    }
    if (encoding == SSE_UTF8) {
        string utf8_buffer = Encoding::defaultEncodingToUTF8(cwdBuffer);
        return Filename(utf8_buffer, FI_HOST, encoding);
    } else {
        return Filename(cwdBuffer, FI_HOST, encoding);
    }
#else
    if (encoding == SSE_UTF8) {
        DWORD size = MAX_PATH;
        GrowArray<wchar_t> path(size);
        DWORD len = GetCurrentDirectoryW(size, path.getArrayNC());

        if (len == 0) {
            throw_XSystem("GetCurrentDirectoryW");
        }
        if (len > size) {
            size = len;
            path.ensureAtLeast(size);
            len = GetCurrentDirectoryW(size, path.getArrayNC());
            if (len == 0) {
                throw_XSystem("GetCurrentDirectoryW (2)");
            }
        }

        return Filename(ConsoleEncode::conv_wide_to_multi_byte_utf8_string(path.getArray()), FI_WINDOWS, encoding);
    } else {
        DWORD size = MAX_PATH;
        GrowArray<char> path(size);
        DWORD len = GetCurrentDirectoryA(size, path.getArrayNC());

        if (len == 0) {
            throw_XSystem("GetCurrentDirectoryA");
        }
        if (len > size) {
            size = len;
            path.ensureAtLeast(size);
            len = GetCurrentDirectoryA(size, path.getArrayNC());
            if (len == 0) {
                throw_XSystem("GetCurrentDirectoryA (2)");
            }
        }

        return Filename(path.getArray(), FI_WINDOWS, encoding);
    }
#endif
}

const Filename &Filename::initialPath(SystemStringEncoding encoding) {
    if (encoding == SSE_UTF8) {
        static bool initialized_utf8 = false;
        static Filename f_utf8(encoding);

        if(!initialized_utf8) {
            initialized_utf8 = true;
            f_utf8 = currentPath(encoding);
        }
        return f_utf8;
    } else {
        static bool initialized_sysdef = false;
        static Filename f_sysdef(encoding);

        if(!initialized_sysdef) {
            initialized_sysdef = true;
            f_sysdef = currentPath(encoding);
        }
        return f_sysdef;
    }
}

bool Filename::hasFilesystem() const
{
    FilenameNode const *n = getNode()->getRoot();

    // skip absolute marker, see if what follows is non-empty
    return n->name[1] != '\0';
}


const char *Filename::getFilesystem() const
{
    FilenameNode const *n = getNode()->getRoot();

    // skip absolute marker
    return n->name + 1;
}


bool Filename::isAbsolute() const
{
    FilenameNode const *n = getNode()->getRoot();

    return n->name[0] == '/';
}

Filename Filename::getRelative() const
{
    if (!isAbsolute() && !hasFilesystem()) return *this;

    Filename res(this->encoding), tmp = *this;
    vector<const char*> components;
    while (tmp.hasNames()) {
        components.push_back(tmp.finalName());
        tmp = tmp.parent();
    }

    while (!components.empty()) {
        res /= components.back();
        components.pop_back();
    }

    return res;
}

bool Filename::hasNames() const
{
    return !getNode()->isRoot();
}


int Filename::numNames() const
{
    return (int)getNode()->depth - 1;
}


const char *Filename::finalName() const
{
    xassert(hasNames());
    return getNode()->name;
}


Filename Filename::parent() const
{
    xassert(hasNames());
    getNode()->parent->incRefct();
    return Filename(getNode()->parent, encoding);
}

inline bool FCIsCaseInsensitive(Filename::FileComparisonFlag flags) {
    return flags & Filename::FCF_CASE_INSENSITIVE;
}

static Filename::FileComparison
fc_strcmp(const char *s1,
          const char *s2,
          bool case_insensitive) {
    int res =
        case_insensitive?strcasecmp(s1, s2):strcmp(s1, s2);
    if (res < 0)
        return Filename::FC_LT;
    if (res > 0)
        return Filename::FC_GT;
    return Filename::FC_EQUAL;
}

Filename::FileComparison Filename::compareInternal
(Filename const &obj,
 FileComparisonFlag flags,
 int *commonPrefixSize) const
{
    assertMatchingEncoding(encoding, obj.encoding);

    FilenameNode *node = this->getNode(), *obj_node = obj.getNode();

    // Optimize the "equal" case even more
    // It's important with the "1-element cache" strategy used e.g. in
    // edg_filename.cpp
    if(node == obj_node) {
        if(commonPrefixSize) {
            *commonPrefixSize = numNames();
        }
        return FC_EQUAL;
    }

    bool case_insensitive =
        FCIsCaseInsensitive(flags);

    if (flags & FCF_FINAL_FIRST) {
        while (node && obj_node) {
            FileComparison res =
                fc_strcmp(node->name, obj_node->name, case_insensitive);
            if (res) { return res; }
            node = node->parent;
            obj_node = obj_node->parent;
        }
        // We should hit differences in names due to root and superroot
        // nodes for names with different numbers of nodes before we
        // hit a single NULL node.
        string_assert(
            !node && !obj_node,
            "Both node and obj_node should be NULL");
        return FC_EQUAL;
    }

    int tDepth = node->depth;
    int oDepth = obj_node->depth;
    // walk up the 'parent' pointers of the deeper one
    // until they are at the same level
    // For loop is probably faster than checking each node's depth, we
    // know the depth is going down 1 every time
    FilenameNode *thisIsoLevel = node;
    FilenameNode *objIsoLevel = obj_node;
    Filename::FileComparison resIfIsoEq;

    // use the absolute minimum number of comparisons
    if(tDepth > oDepth) {
        resIfIsoEq = FC_CHILD;
        do {
            thisIsoLevel = thisIsoLevel->parent;
            --tDepth;
        } while(tDepth > oDepth);
    } else if(oDepth > tDepth) {
        resIfIsoEq = FC_PARENT;
        do {
            objIsoLevel = objIsoLevel->parent;
            --oDepth;
        } while(oDepth > tDepth);
    } else {
        resIfIsoEq = FC_EQUAL;
    }
    
    // kick off a recursive comparison
    FileComparison res = thisIsoLevel->compareToIsoLevel
        (objIsoLevel, case_insensitive, commonPrefixSize);
    if (res) { return res; }
    
    // compare by depth, since one is a prefix of the other
    // (maybe proper, maybe equal)
    return resIfIsoEq;
}

Filename::FileComparison Filename::compare
(Filename const &obj,
 FileComparisonFlag flags) const
{
    return compareInternal(obj, flags, NULL);
}

int Filename::commonPrefixNumNames
(Filename const &obj,
 FileComparisonFlag flags) const
{
    string_assert(
        !(flags & FCF_FINAL_FIRST),
        "Cannot call commonPrefixNumNames with FCF_FINAL_FIRST");
    int rv;
    compareInternal(obj, flags, &rv);
    return rv;
}

size_t Filename::compute_hash(FileComparisonFlag fcf) const {
    return compute_hash(fcf, COV_HASH_INIT);
}

size_t Filename::compute_hash(FileComparisonFlag fcf, size_t h) const {
    if(!hasNames()) {
        if(!FCIsCaseInsensitive(fcf)) {
            return cov_hash_str(lowerCased(getFilesystem()).c_str(), h);
        }
        return cov_hash_str(getFilesystem(), h);
    }
    h = parent().compute_hash(fcf, h);
    
    if(!FCIsCaseInsensitive(fcf)) {
        return cov_hash_str(lowerCased(finalName()), h);
    } else {
        return cov_hash_str(finalName(), h);
    }
    return h;
}

bool Filename::isParent(const Filename &other,
                        FileComparisonFlag fc) const {
    string_assert(
        !(fc & FCF_FINAL_FIRST),
        "Cannot call isParent with FCF_FINAL_FIRST");
    FileComparison r = compare(other, fc);
    return r == FC_EQUAL || r == FC_PARENT;
}

// In this function I specifically do *not* want to allocate anything,
// since comparisons frequently are in inner loops.  The structure of
// the comparison is essentially the above comparison to Filename,
// with the parsing done by the Filename ctor folded into it.  (This
// kind of algorithmic folding in order to eliminate allocation is
// automatable in some languages.)
Filename::FileComparison Filename::compare(char const *str) const
{
    return compare(str, FI_MAGIC, FCF_NONE);
}

Filename::FileComparison Filename::compare(char const *str,
                      FilenameInterp interp,
                      FileComparisonFlag flags) const
{
    string_assert(
        !(flags & FCF_FINAL_FIRST),
        "Cannot call string compare with FCF_FINAL_FIRST");
    bool case_insensitive =
        FCIsCaseInsensitive(flags);

    Filename::FileComparison res;
    // Root will be checked in compareToString

    // recursive comparison to string
    res = getNode()->compareToString
        (str, interp,
         case_insensitive);
    if (res) {
        return res;
    }

    // The above call has compared all the components down to 'node'
    // to their corresponding elements in 'str', but has not taken
    // care of the possibility of there being additional stuff in
    // 'str' beyond what already compared as equal.  So we handle that
    // case now.

    // Handle trailing './'
    skipDots(str, interp);
    
    // exhausted nodes; did we exhaust 'str'?
    if (*str) {
        return FC_PARENT;    // no, so 'this' is a parent
    }
    else {
        return FC_EQUAL;     // yes, they are equal
    }
}


string Filename::toString(FilenameInterp fi) const
{
    return toStringInternal(fi, /*unicodePrefix*/false);
}

string Filename::toStringInternal(FilenameInterp fi, bool unicodePrefix) const
{
    FilenameNode *node = this->getNode();

    // This needs to be fast. We need to call this to do actual file
    // operations.
    // ostringstream is slow
    if(node->isRoot() &&
       node->name[0] != '/' &&
       !node->name[1]) {
        static string dot(".");
        return dot;
    }
    string rv;
    int size = 0;
    int d = node->depth;
    // Allocate on the stack. d is likely to be very small.
    const char *v[d];
    const char **cur_name = v;
    const FilenameNode *n = node;
    for(int i = 0; i < d; ++i) {
        const char *name = n->name;
        *(cur_name++) = name;
        size += strlen(name) + 1;
        n = n->parent;
    }
    const char unicodePrefixStr[] = "\\\\?\\";
    --cur_name;
    const char *root = *cur_name--;
    bool isAbsolute = root[0] == '/';
    // Skip "isAbsolute" marker
    ++root;
    if(unicodePrefix) {
        size += ARRAY_SIZE(unicodePrefixStr) - 1;
    }
    // Take off 2: first "name" has an "absoluteness" char and will
    // not use a separator (unless the path is exactly '/')
    // Take off 1 more if not absolute.
    rv.reserve(size - 2 - (isAbsolute && d > 1));
    if(unicodePrefix) {
        rv.append(unicodePrefixStr);
        if(begins_with(root, "\\\\")) {
            // UNC. In this case, the syntax is
            // \\?\UNC\foo to represent \\foo
            rv.append("UNC");
            // Skip one leading '\'
            rv.append(root + 1);
        } else {
            rv.append(root);
        }
    } else {
        rv.append(root);
    }
    if(isAbsolute) {
        rv += fi==FI_WINDOWS ? '\\' : '/';
    }
    if(cur_name >= v) {
        const char *n = *cur_name--;
        rv.append(n);
        while(cur_name >= v) {
            n = *cur_name--;
            rv += fi==FI_WINDOWS ? '\\' : '/';
            rv += n;
        }
    }
    return rv;
}

string Filename::toWindowsStringWithLongNamePrefix() const
{
    // We need a normalized file name.
    // '..' is not acceptable when using the '\\?\' prefix.
    Filename f = normalized(NF_NONE, /*base*/ NULL);
    return f.toStringInternal(FI_WINDOWS, /*unicodePrefix*/true);
}

string Filename::toSystemDefaultString() const
{
    if (isUTF8Encoded()) {
        return Encoding::defaultEncodingFromUTF8(toString());
    } else {
        return toString();
    }
}

string Filename::toUTF8String() const
{
    if (isUTF8Encoded()) {
        return toString();
    } else {
        return Encoding::defaultEncodingToUTF8(toString());
    }
}

// Only define this on win32; this uses a win32 API
#ifdef __MC_MINGW__
wstring toWindowsStringWithLongNamePrefixUnicode(const Filename &f)
{
    return Encoding::systemEncodingToWindowsUnicode(
        f.toWindowsStringWithLongNamePrefix(),
        f.getEncoding());
}
#endif

string Filename::toHostStyleString() const
{
    return toString(FI_HOST);
}

Filename::operator path() const
{
    sup_assert(!empty());
    return path(toString());
}

ostream& Filename::write(ostream &os) const
{
    if(!hasNames() && !hasFilesystem() && !isAbsolute()) {
        // Special case: empty -> "."
        os << '.';
        return os;
    }
    return getNode()->write(os, '/');
}


void Filename::writeToFILE_xmlEscape(FILE *dest) const
{
    if(!hasNames() && !hasFilesystem() && !isAbsolute()) {
        // Special case: empty -> "."
        xfputc('.', dest);
        return;
    }
    getNode()->writeToFILE_xmlEscape(dest);
}


// ----------------------- FilenameNode ----------------------
static char *copyString(char const *src, int len)
{
    char *ret = new char[len+1];
    memcpy(ret, src, len);
    ret[len] = 0;
    return ret;
}

FilenameNode::FilenameNode(FilenameNode * /*nullable*/ parent_,
                           char const *nameSrc, int nameLen)
  : parent(parent_),
    name(copyString(nameSrc, nameLen)),
    children(0/*4*/ /*initSize*/),
    refct(1),
    depth(parent_? parent_->depth + 1 : 0)
{                
    if (parent) {
        // insert myself into the parent's children array
        parent->insertChild(this, nameLen);
    }

    // Use index off free list, if available.
    if (!freeFilenameNodeIndexes->empty()) {
        index = freeFilenameNodeIndexes->back();
        freeFilenameNodeIndexes->pop_back();
        (*filenameNodes)[index] = this;
    } else {
        index = filenameNodes->size();
        cond_assert(index < MAX_SAFE_FILENAMENODE_INDEX);
        filenameNodes->push_back(this);
    }
}


FilenameNode::~FilenameNode()
{
    try {
        if (parent) {
            parent->removeChild(this);
        }
        xassert(children.isEmpty());
    }
    CAUTIOUS_RELAY

    // If this is the last node in the table,
    // shrink the table.  Otherwise add its index
    // to the free list.
    if (index == (filenameNodes->size()-1)) {
        filenameNodes->pop_back();
    } else {
        freeFilenameNodeIndexes->push_back(index);
        (*filenameNodes)[index] = NULL;
    }

    delete[] name;
}


void FilenameNode::incRefct()
{
    if (refct == MAX_REFCT) {
        // no more incrementing allowed; it's maxed out
    }
    else {
        refct++;
    }
}


void FilenameNode::decRefct()
{
    if (refct == MAX_REFCT) {
        // stuck
    }
    else {
        refct--;
        if (refct == 0) {
            delete this;
        }
    }
}


FilenameNode const *FilenameNode::getRoot() const
{
    FilenameNode const *ret = this;
    while (ret->depth > 1) {
        xassert(ret->parent->depth == ret->depth - 1);
        ret = ret->parent;
        xassert(ret);
    }
    return ret;
}


void FilenameNode::insertChild(FilenameNode *child, int nameLen)
{                             
    // find where to insert it
    int index;
    if (findChildIndex(index, child->name, nameLen)) {
        xfailure("attempt to create a duplicate child node");
    }

    // number of valid entries in the array
    int numOldEntries = children.length();

    // ensure sufficient allocated space in the array for
    // one more element
    children.push(NULL);

    // move the elements at 'index' and above down by one
    {
        FilenameNode **array = children.getArrayNC();
        memmove(array+index+1, array+index, 
                (numOldEntries - index) * sizeof(*array));
    }

    // stick the new one in place
    children[index] = child;

    // the child should already be pointing here, and we need
    // to bump the refct accordingly
    xassert(child->parent == this);
    this->incRefct();
}


void FilenameNode::removeChild(FilenameNode *child)
{
    // find where to remove it
    int index;
    if (!findChildIndex(index, child->name, strlen(child->name))) {
        xfailure("attempt to remove a nonexistent child node");
    }

    // move the elements above 'index' up by one
    {
        FilenameNode **array = children.getArrayNC();
        memmove(array+index, array+index+1, 
                (children.length() - (index+1)) * sizeof(*array));
    }

    // throw away the defunct last element
    children.pop();

    // symmetric with 'insertChild'
    xassert(child->parent == this);
    this->decRefct();
}

// Same as above, except returns a FileComparison and is potentially
// case-insensitive
static Filename::FileComparison
fc_strcmp_len2(const char *s1,
               const char *s2,
               int length,
               bool case_insensitive) {
    int res =
        (case_insensitive?strncasecmp:strncmp)(s1, s2, length);
    if (res < 0)
        return Filename::FC_LT;
    if (res > 0 || s1[length])
        return Filename::FC_GT;
    return Filename::FC_EQUAL;
}

bool FilenameNode::findChildIndex(int &index /*OUT*/,
                                  char const *name, int nameLen)
{
    // Since the array is sorted, use binary search to find the child.
    
    // Bounds on where the child might be.
    int lo = 0;                        // inclusive
    int hi = children.length();        // exclusive

    // Another way to look at lo and hi is as *inclusive* bounds
    // on where to insert the child if it is not there.

    while (lo < hi) {
        // Note that if 'lo' and 'hi' are one apart, then 'mid'
        // will be set to 'lo'.
        int mid = (lo+hi)/2;

        // Compare 'name' to (the name stored in) 'mid'.
        int cmp = strcmp_len2(children[mid]->name, name, nameLen);
        if (cmp == 0) {
            index = mid;
            return true;     // found it
        }
        else if (cmp > 0) {
            // Look in lower half.  Since 'name' is less than 'mid',
            // set the high exclusive bound there.  Alternatively, we
            // might want to insert 'name' at 'mid' (displacing 'mid'
            // upward), but certainly do not want to insert it any
            // higher than 'mid'.
            hi = mid;
        }
        else {
            // Look in upper half.  'name' is greater than 'mid', so
            // cannot be at 'mid', and we would not want to insert the
            // child there either.
            lo = mid+1;
        }
    }
    
    // The process above will not allow 'lo' and 'hi' to cross past
    // each other.
    xassert(lo == hi);
    
    // And their ending location is the right insertion point, based
    // on the loop invariant that they inclusively bound the insertion
    // point.
    index = lo;
    return false;         // did not find
}


FilenameNode *FilenameNode::findChild(char const *name, int nameLen)
{
    int index;
    if (findChildIndex(index, name, nameLen)) {
        return children[index];
    }
    else {
        return NULL;
    }
}


FilenameNode *FilenameNode::getOrCreateChild(char const *name, int nameLen)
{
    FilenameNode *child = findChild(name, nameLen);
    if (!child) {
        // need to create a new child; this bumps the refct of 'this',
        // and sets 'child' refct to 1
        child = new FilenameNode(this, name, nameLen);
    }
    else {
        child->incRefct();
    }
    return child;
}

template<typename T> static Filename::FileComparison fcCompare
(T t1, T t2) {
    if(t1 < t2)
        return Filename::FC_LT;
    if(t1 > t2)
        return Filename::FC_GT;
    return Filename::FC_EQUAL;
}

Filename::FileComparison FilenameNode::compareToIsoLevel
(FilenameNode *other,
 bool case_insensitive,
 int *commonPrefixSize)
{
    if (this == other) {
        if(commonPrefixSize) {
            *commonPrefixSize = depth - 1;
        }
        return Filename::FC_EQUAL;
    }

    xassert(this->depth == other->depth);

    if (this->isRoot()) {
        xassert(other->isRoot());

        // filesystem
        Filename::FileComparison res = fc_strcmp(this->name+1, other->name+1,
                                       case_insensitive);
        if (res) {
            if(commonPrefixSize) {
                *commonPrefixSize = -1;
            }
            return res;
        }
        
        // absolute marker
        // Absolute comes first, while ' ' comes before '/', so invert.
        res = fcCompare(other->name[0], this->name[0]);

        if(commonPrefixSize) {
            *commonPrefixSize = res ? -1 : 0;
        }
        return res;
    }

    // compare parents
    Filename::FileComparison res =
        this->parent->compareToIsoLevel(
            other->parent,
            case_insensitive,
            commonPrefixSize);
    if (res) { return res; }

    // names at this level
    Filename::FileComparison rv =
        fc_strcmp(this->name, other->name, case_insensitive);
    if(commonPrefixSize && !rv) {
        ++*commonPrefixSize;
    }
    return rv;
}


// This method works by recursing all the way up to the parent, then
// comparing to 'str' while popping out of recursion.  As each part of
// 'str' is compared, it is advanced.  So, we effectively work from
// root to leaf in both representations, comparing corresponding
// parts.
Filename::FileComparison FilenameNode::compareToString
(char const *&str,
 Filename::FilenameInterp interp,
 bool case_insensitive)
{
    if (isRoot()) {
        // pull off the root designator
        char const *end_of_fs;
        char const *beginning_of_fs;
        scoped_array<char> allocated_fs;
        bool absolute;
        parseNameStart(str,
                       allocated_fs,
                       beginning_of_fs,
                       end_of_fs,
                       absolute,
                       interp);


        // filesystem name
        Filename::FileComparison res = fc_strcmp_len2
            (name + 1,
             beginning_of_fs,
             end_of_fs - beginning_of_fs,
             case_insensitive);
        if (res) { return res; }

        // absolute marker
        // absolute comes first, so test the "relative" flag
        res = fcCompare((int)(name[0]==' '),
                        (int)(absolute==false));
                        
        return res;
    }

    // compare parents
    Filename::FileComparison res = parent->compareToString(str,
                                                           interp,
                                                           case_insensitive);
    if (res) {
        return res;
    }

    char const *end;
    const char *next;

    findNextComponent(str, end, next, interp);

    if(str == end) {
        // exhausted 'str' before exhausting 'this'
        return Filename::FC_CHILD;
    }

    int length = end - str;

    // name before separator
    res = fc_strcmp_len2(name, str, length, case_insensitive);

    // advance 'str' for child node's comparison
    str = next;

    return res;
}


ostream& FilenameNode::write(ostream &os, char sep) const
{
    if (isRoot()) {
        // filesystem
        os << name+1;

        // absolute?
        if (name[0] == '/') {
            // we store a forward slash internally, but print whatever
            // separator we're told to use.
            os << sep;
        }
    }
    else {
        parent->write(os, sep);

        // separator if we're not just after the root
        if (!parent->isRoot()) {
            os << sep;
        }

        os << name;
    }

    return os;
}


void FilenameNode::writeToFILE_xmlEscape(FILE *dest) const
{
    if (isRoot()) {
        // filesystem
        xfputs_xmlEscape(name+1, dest);

        // absolute?
        if (name[0] == '/') {
            xfputc('/', dest);
        }
    }
    else {
        parent->writeToFILE_xmlEscape(dest);

        // separator if we're not just after the root
        if (!parent->isRoot()) {
            xfputc('/', dest);
        }

        xfputs_xmlEscape(name, dest);
    }
}


long FilenameNode::estimateMemUsage() const
{
    // Estimate of the amount of overhead associated with heap
    // allocation.  One word is what Doug Lea's malloc requires.
    enum { HEAP_OVERHEAD = sizeof(void*) };

    long ret = sizeof(*this) + HEAP_OVERHEAD;

    ret += strlen(name) + 1 + HEAP_OVERHEAD;

    if (children.size()) {
        ret += children.size() * sizeof(FilenameNode*) + HEAP_OVERHEAD;
    }
    for (int i=0; i < children.length(); i++) {
        ret += children[i]->estimateMemUsage();
    }

    return ret;
}


void FilenameNode::trimMemUsage()
{
    children.consolidate();
    for (int i=0; i < children.length(); i++) {
        children[i]->trimMemUsage();
    }
}


STATICDEF FilenameNode *FilenameNode::getFilenameNode(Filename const &fn)
{
    return fn.getNode();
}


// ----------------------- global funcs ----------------------
long estimateFilenameMemoryUsage()
{
    size_t rsl = 0;

    if (filenameNodes) {
        // Overhead of indexing structures
        rsl += vector_memory(*filenameNodes);
        rsl += vector_memory(*freeFilenameNodeIndexes);

        // Actual memory of nodes
        rsl += getSuperRootNode()->estimateMemUsage();
    }

    return rsl;
}


void trimFilenameMemoryUsage(Filename const *subtree)
{
    if (subtree) {
        FilenameNode::getFilenameNode(*subtree)->trimMemUsage();
    }
    else if (filenameNodes) {
        getSuperRootNode()->trimMemUsage();
    }
}


FilenameNode *getSuperRootNode()
{
    if (!filenameNodes) {
        // initialize node list, and free index list
        filenameNodes = new vector<FilenameNode *>;
        freeFilenameNodeIndexes = new vector<FilenameNodeIndex>;

        // create super-root node
        filenameNodes->push_back( new FilenameNode(NULL /*parent*/, "superRoot", 9) );
    }
    return filenameNodes->front();
}


FilenameNode *getRootNode(char const *fs,
                          int fsLen,
                          bool absolute)
{
    // I definitely do *not* want to do any heap allocation in this
    // routine, since it will be called frequently.  One approach
    // would be to copy or generalize 'findChildIndex' to accept a
    // bool 'absolute'.  Instead, I'll rely on alloca.
    char *fsWithAbsFlag = (char*)alloca(fsLen + 2);
    fsWithAbsFlag[0] = absolute? '/' : ' ';
    memcpy(fsWithAbsFlag+1, fs, fsLen);

    return getSuperRootNode()->getOrCreateChild(fsWithAbsFlag, fsLen+1);
}

#ifdef __MC_MINGW__
static time_t FTIME_to_time_t(FILETIME ftime) {
    long long t = ftime.dwHighDateTime;
    t <<= 32;
    t |= ftime.dwLowDateTime;
    // Convert to seconds
    t /= 10000000;
    // Remove number of seconds between 1601/01/01 and Unix epoch
    t -= 11644473600LL;
    return t;
}

static FILETIME time_t_to_FTIME(time_t t) {
    // Add number of seconds between 1601/01/01 and Unix epoch
    unsigned long long longTime = t;
    longTime += 11644473600LL;
    // Convert from seconds to hundreds of ns.
    longTime *= 10000000;
    FILETIME ftime;
    ftime.dwLowDateTime = longTime & 0xffffffff; // 2^32 - 1
    ftime.dwHighDateTime = longTime >> 32;
    return ftime;
}
#endif

#ifndef __MC_MINGW__
static void stat_to_FileStats
(struct
#ifdef HAVE_STAT64
 stat64
#else
 stat
#endif
 &sb,
 FileStats &buf) {
    if(S_ISDIR(sb.st_mode)) {
        buf.kind = FileStats::FK_DIRECTORY;
    } else if(S_ISREG(sb.st_mode)) {
        buf.kind = FileStats::FK_REGULAR;
    } else if(S_ISLNK(sb.st_mode)) {
        buf.kind = FileStats::FK_LINK;
    } else {
        buf.kind = FileStats::FK_OTHER;
    }
    buf.size = sb.st_size;
    buf.last_access = sb.st_atime;
    buf.last_modification = sb.st_mtime;
    buf.io_buf_size = sb.st_blksize;
}
#endif

#ifdef __MC_MINGW__
static void WIN32_FILE_ATTRIBUTE_DATA_to_FileStats(
    const WIN32_FILE_ATTRIBUTE_DATA &fileInfo,
    FileStats &buf)
{
    if(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        buf.kind = FileStats::FK_DIRECTORY;
    } else {
        // On Windows, there are 2 kinds of files: directories and regular
        // files.
        // Consider there are no symlinks (although they were introduced
        // in Vista)
        buf.kind = FileStats::FK_REGULAR;
    }
    buf.size = fileInfo.nFileSizeHigh;
    buf.size <<= 32;
    buf.size |= fileInfo.nFileSizeLow;
    buf.last_access = FTIME_to_time_t(fileInfo.ftLastAccessTime);
    buf.last_modification = FTIME_to_time_t(fileInfo.ftLastWriteTime);
}
#endif

static
bool getFileStatsEncoded(
    const EncodedFileName &f,
    FileStats &buf) {
#ifdef __MC_MINGW__
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if(!CALL_WIN_API_STR(GetFileAttributesEx, f,
                         GetFileExInfoStandard,
                         &fileInfo)) {
        // BZ 29030. We've overridden cov-emit's 'fopen' calls with
        // an indirect call to this. cov-emit depends on errno being
        // set correctly, however, so let's set it here.
        errno = getErrnoFromWindowsError(GetLastError());
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA_to_FileStats(fileInfo, buf);
    return true;
#else
    int rv;
#  ifdef HAVE_STAT64
    struct stat64 sb;
    rv = stat64(f.adata, &sb);
#  else
    struct stat sb;
    rv = stat(f.adata, &sb);
#  endif
    if(rv != 0) {
        // Some of our code (e.g. emit-db) iterates over the files in
        // the current directory and tries <file>/emit-db
        // This causes an ENOTDIR, so handle it the same as an ENOENT.
        if(errno == ENOENT || errno == ENOTDIR) {
            return false;
        }
        throw_XSystem("stat", f.adata);
    }
    stat_to_FileStats(sb, buf);
    return true;
#endif
}

bool getFileStats(const Filename &f, FileStats &buf) {
    return getFileStatsEncoded(ENCODED_FILE_NAME(f), buf);
}

bool getFileStats(const char *f, FileStats &buf) {
    return getFileStatsEncoded(EncodedFileName(f), buf);
}

bool getLinkStats(const char *f, FileStats &buf) {
#ifdef __MC_MINGW__
    return getFileStats(f, buf);
#else
    int rv;
#  ifdef HAVE_STAT64
    struct stat64 sb;
    rv = lstat64(f, &sb);
#  else
    struct stat sb;
    rv = lstat(f, &sb);
#  endif
    if(rv != 0) {
        if(errno == ENOENT || errno == ENOTDIR) {
            return false;
        }
        throw_XSystem("lstat", f);
    }
    stat_to_FileStats(sb, buf);
    return true;
#endif
}

bool getLinkStats(const Filename &f, FileStats &buf) {
#ifdef __MC_MINGW__
    return getFileStats(f, buf);
#else
    return getLinkStats(f.toSystemDefaultString().c_str(), buf);
#endif
}

// Used and tested in libs/auth-key.
bool is_readable_only_by_owner(Filename const &fileName)
{
    const EncodedFileName &f = ENCODED_FILE_NAME(fileName);
#ifdef __MC_MINGW__
    return true;    // TODO
#else
    int rv;
#  ifdef HAVE_STAT64
    struct stat64 sb;
    rv = stat64(f.adata, &sb);
#  else
    struct stat sb;
    rv = stat(f.adata, &sb);
#  endif
    if(rv != 0) {
        // Some of our code (e.g. emit-db) iterates over the files in
        // the current directory and tries <file>/emit-db
        // This causes an ENOTDIR, so handle it the same as an ENOENT.
        if(errno == ENOENT || errno == ENOTDIR) {
            return false;
        }
        throw_XSystem("stat", f.adata);
    }
    return (sb.st_mode & (S_IRGRP|S_IROTH)) == 0;
#endif
}


::time_t get_last_write_time(const Filename &file)
{
    FileStats buf;
    if(!getFileStats(file, buf)) {
        return 0;
    }
    return buf.last_modification;
}

static bool setLastWriteTimeEncoded(
    const EncodedFileName &encodedName,
    ::time_t when)
{
#ifdef __MC_MINGW__
    FILETIME lastWriteTime = time_t_to_FTIME(when);
    HANDLE fileHandle = CALL_WIN_API_STR(CreateFile, encodedName,
        GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(fileHandle == INVALID_HANDLE_VALUE){
        return false;
    }
    bool result = ::SetFileTime(fileHandle, NULL, NULL, &lastWriteTime);
    ::CloseHandle(fileHandle);
    return result;
#else
    struct utimbuf utimes;
    // We have to set the access time to something since it's in the same
    // buffer as the modtime.  I'm told the below is what touch(1) does,
    // so I'm doing the same.
    utimes.actime = when;
    utimes.modtime = when;
    return utime(encodedName.adata, &utimes) == 0;
#endif
}

bool set_last_write_time(const Filename &file, ::time_t when){
    return setLastWriteTimeEncoded(ENCODED_FILE_NAME(file), when);
}

bool set_last_write_time_now(const Filename &file) {
    return setLastWriteTimeEncoded(ENCODED_FILE_NAME(file), ::time(NULL));
}

bool dir_exists(const Filename &f) {
    FileStats buf;
    return getFileStats(f, buf) &&
        buf.is_dir();
}

bool dir_exists_l(const Filename &f) {
    FileStats buf;
    return getLinkStats(f, buf) &&
        buf.is_dir();
}

bool file_exists(const Filename &f) {
    FileStats buf;
    return getFileStats(f, buf) &&
        buf.is_reg();
}

bool file_exists_l(const Filename &f) {
    FileStats buf;
    return getLinkStats(f, buf) &&
        !buf.is_dir();
}

bool exists(const Filename &f)
{
    return dir_exists(f) || file_exists(f);
}

unsigned long long file_size(const Filename &f)
{
    FileStats buf;
    if (!getFileStats(f, buf)) {
        throw_XSystem("stat", f.toString());
    }
    return buf.size;
}

bool remove(const Filename &f)
{
    // You'd think that a simple call to ::remove would work, but ::remove on
    // Windows is ::unlink, i.e. it doesn't work on directories.
#ifndef _WIN32
    if (dir_exists_l(f)) {
        return ::rmdir(f.toSystemDefaultString().c_str()) == 0;
    } else if (file_exists_l(f)) {
        return ::unlink(f.toSystemDefaultString().c_str()) == 0;
    }
#else
    if (dir_exists_l(f)) {
        return CALL_WIN_API(RemoveDirectory, f);
    } else if (file_exists_l(f)) {
        return CALL_WIN_API(DeleteFile, f);
    }
#endif
    return false;
}

unsigned remove_all(const Filename &dir)
{
    unsigned count = 0;
    if (dir_exists_l(dir)) {
        DirEntries de(dir);
        vector<Filename> entries(de.begin(), de.end());
        foreach (it, entries) {
            count += remove_all(*it);
        }
    }
    return count + remove(dir);
}

void create_directory(const Filename &f)
{
#ifndef _WIN32
    if (::mkdir(f.toSystemDefaultString().c_str(), S_IRWXU|S_IRWXG|S_IRWXO) != 0) {
        if (errno == EEXIST) {
            // can't check if dir_exists beforehand because of possible race
            // conditions
            if (dir_exists(f)) {
                return;
            }
        }
        throw_XSystem("mkdir", f.toString());
    }
#else
    int rv = CALL_WIN_API(CreateDirectory, f,
                          /*lpSecurityAttributes*/ NULL);
    if (rv == 0) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (dir_exists(f)) {
                return;
            }
        }
        throw_XSystem("CreateDirectory", f.toString());
    }
#endif
}

void create_directories(const Filename &f)
{
    if (!f.hasNames()) {
        // trying to create root directory (/) or current directory (.)
        return;
    }

    if (!dir_exists(f.parent()))
        create_directories(f.parent());
    create_directory(f);
}

void copy_file(const Filename &src, const Filename &dest)
{
    eifstream src_stream(src, eifstream::binary);
    eofstream dest_stream(dest, eofstream::binary);

    if (copy_stream(dest_stream, src_stream) != file_size(src)) {
        throw_XMsg(stringbc("Couldn't copy " << src << " to " << dest));
    }
}

void copy_link(const Filename &src, const Filename &dest) {
#if !defined(__MC_MINGW__)
    char buf[PATH_MAX];
    int err;
    errno = 0;
    int rv = readlink(src.toSystemDefaultString().c_str(), buf, PATH_MAX - 1);
    err = errno;
    if (rv > 0) {
      errno = 0;
      if (symlink(buf, dest.toSystemDefaultString().c_str()) != 0) {
        err = errno;
        throw_XMsg(stringbc("Couldn't copy link " << src << " to " << dest << ": " << strerror(err)));
      }
    } else {
      throw_XMsg(stringbc("Couldn't copy link " << src << " to " << dest << ": " << strerror(err)));
    }
#endif
}


bool copy_files(const Filename &from, const Filename &to)
{
    bool copied_any_files = false;

    if (!dir_exists(from)) {
        throw_XMsg(stringbc("Couldn't copy files from directory " <<
                            from << " because it does not exist."));
    }
    create_directories(to);

    foreach(it, dir_entries(from)) {
        FileStats stats;
        if (getLinkStats(*it, stats)) {
            if (stats.is_dir()) {
                // recurse into directory
                bool res = copy_files(from / it->finalName(), to / it->finalName());
                copied_any_files = (copied_any_files || res);
            } else if (stats.is_reg()) {
                // copy regular file
                copy_file(from / it->finalName(), to / it->finalName());
                copied_any_files = true;
            } else if (stats.is_link()) {
                // a symlink (possibly broken)
                copy_link(from / it->finalName(), to / it->finalName());
                copied_any_files = true;
            } else {
                // something else (char device, etc.) - skip
            }
        } else {
            throw_XMsg(stringbc("Couldn't stat " << *it));
        }
    }

    return copied_any_files;
}

bool check_writeable(const Filename& p)
{
    if (!dir_exists(p)) {
        // If we can create the directory, we can write to it ...
        create_directories(p);
    }

    // get_unique_string is in "system",
    // which depends on this file...
    ostringstream unique_string_stream;
    unique_string_stream << "check-writeable-";
#ifdef __MC_MINGW__
    unique_string_stream << (unsigned)GetCurrentProcessId();
#else
    unique_string_stream << (unsigned)getpid();
#endif

    Filename tmp_path = p / unique_string_stream.str();
    try {
        eofstream o(tmp_path, eofstream::text);
        o << "HELLO WORLD" << endl;
        o.flush();
        if(o.fail()) {
            // Make sure we remove the file in all cases, even if
            // we believe we couldn't write it (e.g. disk full or
            // some other non-open error)
            remove(tmp_path);
            return false;
        }
        o.close();
        remove(tmp_path);
        return true;
    } catch(XSystem &e) {
        COV_CAUGHT(e);
        // It doesn't hurt to remove here either.
        remove(tmp_path);
        return false;
    }
}

bool atomic_rename(const Filename &from_file, const Filename &to_file)
{
    assertMatchingEncoding(from_file.getEncoding(), to_file.getEncoding());
#ifdef _WIN32
    /* Note: the Windows implementation of atomic_rename() isn't atomic.
       MoveFileTransacted() is atomic, but Microsoft states it may not be
       available in future versions of Windows. */
    if (from_file.isUTF8Encoded()) {
        return MoveFileExW(toWindowsStringWithLongNamePrefixUnicode(from_file).c_str(),
                           toWindowsStringWithLongNamePrefixUnicode(to_file).c_str(),
                           /*dwFlags*/0);
    } else {
        return MoveFileExA(from_file.toString().c_str(),
                           to_file.toString().c_str(),
                           /*dwFlags*/0);
    }
#else
    string from_str = from_file.toSystemDefaultString();
    string to_str = to_file.toSystemDefaultString();
    const char *from = from_str.c_str();
    const char *to = to_str.c_str();

    GDOUT(locks, "Linking " << from << " to " << to);

    // Ignore the return value.  This will create the lock file,
    // but we cannot rely on the return value from link() because
    // it is unreliable over NFS.  So we do a stat() afterwards to
    // see if the link succeeds.
    int rv = link(from, to);

    BEGIN_IF_DEBUG(locks) {
        if(rv == 0)
            cout << "Link succeeded" << endl;
        else {
            int err = errno;
            cout << "Link failed: " << strerror(err) << endl;
        }
    }
    END_IF_DEBUG(locks);

    struct stat sbuf;
    if(stat(from, &sbuf) == 0) {
        if(sbuf.st_nlink == 2) {
            // The link succeeded.
            if(unlink(from)) {
                int err = errno;
                GDOUT(locks, "Couldn't unlink origin file: "
                      << strerror(err));
            }
            return true;
        } else {
            GDOUT(locks, "Lock not acquired.  nlinks = " << sbuf.st_nlink);
        }
    } else {
        BEGIN_IF_DEBUG(locks);
        int err = errno;
        GDOUT(locks, "Stat failed: "
              << strerror(err));
        END_IF_DEBUG(locks);
    }
    // Hopefully link failed. Otherwise we're left with a file no one
    // knows about.

    return false;
#endif
}

bool same_file(const Filename &f1, const Filename &f2)
{
    Filename tmp1 = f1.normalized(Filename::NF_RESOLVE_SYMLINKS
                                  | Filename::NF_LOWERCASE_IF_WINDOWS
                                  | Filename::NF_SHORTNAME,
                                  /*base*/ NULL);
    Filename tmp2 = f2.normalized(Filename::NF_RESOLVE_SYMLINKS
                                  | Filename::NF_LOWERCASE_IF_WINDOWS
                                  | Filename::NF_SHORTNAME,
                                  /*base*/ NULL);
    return tmp1 == tmp2;
}

void change_dir(const Filename &dir)
{
    // This might throw on a big file, but it would be an error anyway
    FileStats buf;
    if (!getFileStats(dir, buf))
        throw_XMsg(stringb('"' << dir << "\" not found"));
    if (!buf.is_dir())
        throw_XMsg(stringb('"' << dir << "\" is not a directory"));
#ifndef _WIN32
    if (chdir(dir.toSystemDefaultString().c_str()) != 0)
        throw_XSystem("chdir", dir.toString());
#else
    if (CALL_WIN_API(SetCurrentDirectory, dir) == 0) {
        throw_XSystem("SetCurrentDirectory", dir.toString());
    }
#endif
}

bool is_subdirectory(const Filename &f1, const Filename &f2)
{
    Filename tmp1 = f1.normalized(Filename::NF_RESOLVE_SYMLINKS
                                  | Filename::NF_LOWERCASE_IF_WINDOWS
                                  | Filename::NF_SHORTNAME,
                                  /*base*/ NULL);
    Filename tmp2 = f2.normalized(Filename::NF_RESOLVE_SYMLINKS
                                  | Filename::NF_LOWERCASE_IF_WINDOWS
                                  | Filename::NF_SHORTNAME,
                                  /*base*/ NULL);
    return tmp2.compare(tmp1) == Filename::FC_PARENT;
}

void
set_file_writable(const Filename &f)
{
#ifndef _WIN32
    string tmp(f.toSystemDefaultString());
    const char *name = tmp.c_str();
    // Do not attempt to make symlinks writable.
    struct stat stats;
    if (lstat(name, &stats)) {
        throw_XSystem("lstat", name);
    }

    if (S_ISLNK(stats.st_mode))
        return;

    // For directories, all set the read/execute bits because we will
    // likely want to read the directory entries if we want to write
    // to it
    mode_t mode = stats.st_mode | S_IWUSR;
    if (S_ISDIR(stats.st_mode)) {
        mode |= S_IRUSR | S_IXUSR;
    }

    if (chmod(name, mode)) {
        throw_XSystem("chmod", name);
    }
#else
    // SetFileAttributes is vaild for directories as well and the only
    // pertinent attribute seems to be (the lack of) read-only for
    // both files and directories
    if (f.isUTF8Encoded()) {
        wstring tmp(toWindowsStringWithLongNamePrefixUnicode(f));
        const wchar_t *name = tmp.c_str();
        if (!SetFileAttributesW(name, GetFileAttributesW(name) & (~FILE_ATTRIBUTE_READONLY))) {
            throw_XSystem("SetFileAttributesW", f.toString());
        }
    } else {
        string tmp(f.toString());
        const char *name = tmp.c_str();
        if (!SetFileAttributes(name, GetFileAttributes(name) & (~FILE_ATTRIBUTE_READONLY))) {
            throw_XSystem("SetFileAttributes", f.toString());
        }
    }
#endif
}

void
set_file_read_only(const Filename &f)
{
#ifndef _WIN32
    string tmp(f.toSystemDefaultString());
    const char *name = tmp.c_str();
    struct stat stats;
    if (lstat(name, &stats)) {
        throw_XSystem("lstat", name);
    }

    if (S_ISLNK(stats.st_mode))
        return;

    if (chmod(name, stats.st_mode & (~S_IWUSR))) {
        throw_XSystem("chmod", name);
    }
#else
    if (f.isUTF8Encoded()) {
        wstring tmp(toWindowsStringWithLongNamePrefixUnicode(f));
        const wchar_t * name = tmp.c_str();
        if (!SetFileAttributesW(name, GetFileAttributesW(name) | FILE_ATTRIBUTE_READONLY)) {
            throw_XSystem("SetFileAttributesW", f.toString());
        }
    } else {
        string tmp(f.toString());
        const char * name = tmp.c_str();
        if (!SetFileAttributes(name, GetFileAttributes(name) | FILE_ATTRIBUTE_READONLY)) {
            throw_XSystem("SetFileAttributes", f.toString());
        }
    }
#endif
}

void
force_remove(const Filename &f)
{
    if (file_exists_l(f)) {
        set_file_writable(f);
        remove(f);
    }
}

void
force_remove_all(const Filename &dir)
{
    if (dir_exists_l(dir)) {
        set_file_writable(dir);

        DirEntries de(dir);
        vector<Filename> entries(de.begin(), de.end());
        foreach (it, entries) {
            force_remove_all(*it);
        }

        remove(dir);
    } else {
        force_remove(dir);
    }
}

// Open a file to which subsequent output will be redirected,
// returning its file descriptor.
static int open_redir_file(string const &filename)
{
    // SGM 2006-04-19: Originally, this had used _sopen on Windows,
    // and passed O_BINARY.  The former is a gratuitous
    // nonportability, and the latter is something I removed for DR
    // 3722.  So now the code is the same on Windows and unix.
    int fd = open(filename.c_str(),
                  O_WRONLY | O_APPEND | O_CREAT,
                  0666);
    if (fd < 0) {
        cerr << filename << ": open: " << get_last_OS_error_string() << endl;
        return -1;
    }
    else {
        return fd;
    }
}

// Redirect all output.
static bool redirect_output(const Filename &fpath,
                            bool from_stdout,
                            bool from_stderr)
{
    string_assert(fpath.getEncoding() != SSE_UTF8,
        "redirect_output can only be used with system default encoded file names");

    string filename = fpath.toString();

    if(from_stdout) {
        fflush(stdout);
        cout.flush();
    }
    if(from_stderr) {
        fflush(stderr);
        cerr.flush();
    }

    int to_fd = -1;

    // If the destination is one of stdout or stderr, then just
    // redirect the other one to it.
    // Only do that if it's not the FD we're actually redirecting (in
    // which case, the name will be considered to represent a file. I
    // don't know if it's a good idea but that's how it used to work)
    if (from_stderr && filename == "stdout") {
        from_stdout = false;
        to_fd = STDOUT_FILENO;
    } else if (from_stdout && filename == "stderr") {
        from_stderr = false;
        to_fd = STDERR_FILENO;
    }

    bool close_fd = false;

    if(to_fd == -1) {
        to_fd = open_redir_file(filename);
        if (to_fd < 0) {
            return false;     // error already reported
        }
        close_fd = true;
    }

    bool rv;

    if (from_stdout && dup2(to_fd, STDOUT_FILENO) < 0) {
        cerr << "redirect_all_output: dup2 stdout: "
             << get_last_OS_error_string() << endl;
        rv = false;
    } else if (from_stderr && dup2(to_fd, STDERR_FILENO) < 0) {
        cerr << "redirect_all_output: dup2 stdout: "
             << get_last_OS_error_string() << endl;
        rv = false;
    } else {
        rv = true;
    }

    if(close_fd) {
        close(to_fd);
    }
    return rv;
}

bool redirect_all_output(const Filename &fpath)
{
    return redirect_output(fpath, /*from_stdout*/true, /*from_stderr*/true);
}

bool redirect_stderr(const Filename& fpath)
{
    return redirect_output(fpath, /*from_stdout*/false, /*from_stderr*/true);
}

bool redirect_stdout(const Filename& fpath)
{
    return redirect_output(fpath, /*from_stdout*/true, /*from_stderr*/false);
}

void rename(const Filename &from_file, const Filename &to_file)
{
    assertMatchingEncoding(from_file.getEncoding(), to_file.getEncoding());
    // FIXME: Try to make all renames atomic, if we can help it.  I see no
    // point in maintaining an "unsafe" version of something that isn't done
    // very frequently anyway.  Unfortunately atomic_rename() won't work for
    // directories, so we'll just use ::rename for now.
#if 0
    if (!atomic_rename(from_file, to_file)) {
        throw_XSystem("rename", stringb(from_file << " to " << to_file));
    }
#endif
#ifdef _WIN32
    if (from_file.isUTF8Encoded()) {
        if (!MoveFileExW(toWindowsStringWithLongNamePrefixUnicode(from_file).c_str(),
                         toWindowsStringWithLongNamePrefixUnicode(to_file).c_str(),
                         /*dwFlags*/0)) {
            throw_XSystem("MoveFileExW", stringb(from_file << " to " << to_file));
        }
    } else {
        if (!MoveFileExA(from_file.toString().c_str(),
                         to_file.toString().c_str(),
                        /*dwFlags*/0)) {
            throw_XSystem("MoveFileExA", stringb(from_file << " to " << to_file));
        }
    }
#else
    if (::rename(
        from_file.toSystemDefaultString().c_str(),
        to_file.toSystemDefaultString().c_str()))
    {
        throw_XSystem("rename", stringb(from_file << " to " << to_file));
    }
#endif
}

// Make "p" relative to "to", comparing name using "fcf" flags.
// This is used for testing.
static
Filename inner_make_relative(
    const Filename &p,
    const Filename &to,
    Filename::FileComparisonFlag fcf,
    const Filename *base)
{
   assertMatchingEncoding(p.getEncoding(), to.getEncoding());

   Filename p_copy = p.normalized(Filename::NF_NONE, base);
   Filename to_copy = to.normalized(Filename::NF_NONE, base);

   // Count common components (including filesystem in count)
   int common = p_copy.commonPrefixNumNames(to_copy, fcf);
   if(common <= 0) {
       // Different filesystem, or no common components.
       return p_copy;
   }
   // Components in the result, with closest to root last.
   vector<string> components;
   while(p_copy.numNames() > common) {
       components.push_back(p_copy.finalName());
       p_copy = p_copy.parent();
   }

   Filename result(p.getEncoding());

   unsigned rel_depth = to_copy.numNames() - common;

   for (unsigned i=0; i<rel_depth; i++)
       result /= "..";

   while (!components.empty()) {
       result /= components.back();
       components.pop_back();
   }
   return result;
}

Filename make_relative(
    const Filename &p,
    const Filename &to,
    bool case_sensitive_windows) {
    return inner_make_relative(
        p,
        to,
        case_sensitive_windows ? Filename::FCF_NONE : Filename::FCF_CASE_INSENSITIVE_IF_WINDOWS,
        /*base*/NULL
    );
}

Filename make_relative(const Filename &p, const Filename &to)
{
    return make_relative(p, to, false);
}

#ifdef _WIN32
Filename get_windows_short_name(const Filename &long_path)
{
    string_assert(
        long_path.getEncoding() != SSE_UTF8,
        "get_windows_short_name can only be used with system default encoded file names");

    DWORD short_size = MAX_PATH;

    GrowArray<char> short_path(short_size);

    Filename p = long_path.normalized(Filename::NF_NONE, /*base*/ NULL);

    DWORD short_path_len = GetShortPathName(p.toString().c_str(),
                                            short_path.getArrayNC(),
                                            short_size);

    if(short_path_len == 0) {
        throw_XSystem("GetShortPathName", p.toString());
    }
    if (short_path_len > short_size) {
        short_size = short_path_len;
        short_path.ensureAtLeast(short_size);
        short_path_len = GetShortPathName(p.toString().c_str(),
                                          short_path.getArrayNC(),
                                          short_size);
        if(short_path_len == 0) {
            throw_XSystem("GetShortPathName (2)", p.toString());
        }
    }

    return Filename(short_path.getArray(), Filename::FI_WINDOWS);
}
#endif

DEFINE_ISTRING_INSERTER(Filename const &);

// ------------------------- DirEntries ---------------------------

class DirEntries::iterator::impl {
public:
    impl(const Filename &);
    impl(const impl &);
    ~impl();

    // Is there a file name at the current position (i.e. not at end)?
    // starts the iteration if necessary.
    bool hasEntry();
    // Move one position, and return the new hasEntry()
    // starts the iteration if necessary.
    bool nextEntry();
    // Return the current file name.
    // starts the iteration if necessary.
    const Filename& getEntry();
    // Start the iteration (if not started), and returns hasEntry()
    bool start();

    // Free the system data structures.
    void cleanup();

    // Indicates whether this is "." or "..", which we are not
    // interested in and should skip.
    static bool isDotDir(const char *name) {
        if(name[0] != '.')
            return false;
        return name[1] == '\0' || (name[1] == '.' && name[2] == '\0');
    }

    // On Win32, we can an array, not a pointer, so we need to
    // have this array on the caller's stack.
#ifdef _WIN32
    typedef WIN32_FIND_DATA GetNextEntryArg;
    typedef _WIN32_FIND_DATAW GetNextEntryArgW;
#else
    // Outside win32, we don't need anything. Use any random type.
    typedef int GetNextEntryArg;
    typedef int GetNextEntryArgW;
#endif


    // Get the name for the next entry, if any, or return false (in
    // which case "cleanup" should be called)
    bool getNextEntryName(
        const char *&name,
        GetNextEntryArg &arg,
        GetNextEntryArgW &argw
    );

    // We make all the data public; this entire structure is private
    // to this field

    // System data structures for dir iteration
#ifndef _WIN32
    DIR              *dhandle_;
#else
    HANDLE            dhandle_;
#endif

    // Directory we're iterating on
    Filename          base_;
    // File currently being iterated on.
    Filename          filename_;
    // If false, there are no files left in the directory.
    bool              haveFilename_;
    // True if we have started the iteration (i.e. created the system
    // data structures).
    // This only happens the first time we get an entry, increment or
    // compare.
    // The goal is to avoid having to copy the system data structures
    // from an iterator we're not going to look at, e.g. for a call
    // like
    // foo(d.begin(), d.end())
    // where copy-on-write still happens because of the reference in
    // the caller to "begin()"
    bool started_;

#ifdef _WIN32
    // The _WIN32_FIND_DATAW cFileName, converted to UTF-8 if using
    // UTF-8 file names.
    string entry_utf8;
#endif
};

DirEntries::iterator::impl::impl(const Filename &f)
    : base_(f),
      filename_(f.getEncoding()),
      haveFilename_(true),
      started_(false)
{
    // This looks stupid but the "dhandle_" variables have different
    // types, so I want them to have different initializers, in concept.
#ifndef _WIN32
    dhandle_ = NULL;
#else
    dhandle_ = NULL;
#endif
}

bool DirEntries::iterator::impl::start() {
    if(started_)
        return haveFilename_;
#ifndef _WIN32
    if ((dhandle_ = opendir(base_.toSystemDefaultString().c_str()))) {
        // Set started to true *after* we've successfully created the
        // data structures, so that we don't attempt to clean them up
        // in the dtor otherwise.
        // However, we need to set it before calling nextEntry() to
        // avoid a recursive loop.
        started_ = true;
        nextEntry();
    } else {
        throw_XSystem("opendir", base_.toString());
    }
#else
    // This code is different from the above because "FindFirstFile"
    // will actually obtain the first file while "opendir" doesn't.
    if (base_.isUTF8Encoded()) {
        _WIN32_FIND_DATAW entry;
        if ((dhandle_ = FindFirstFileW(
                toWindowsStringWithLongNamePrefixUnicode(base_/"*").c_str(),
                &entry))
            != INVALID_HANDLE_VALUE)
        {
            entry_utf8 = ConsoleEncode::conv_wide_to_multi_byte_utf8_string(entry.cFileName);
            started_ = true;
            if (isDotDir(entry_utf8.c_str())) {
                nextEntry();
            } else {
                // I don't think the first entry is ever anything besides
                // "." so this is probably not reachable.
                // Still, this is how it would be written (see similar
                // code in nextEntry)
                filename_ = base_ / entry_utf8;
                return haveFilename_ = true;
            }
        } else {
            throw_XSystem("FindFirstFileW", base_.toString());
        }
    } else {
        WIN32_FIND_DATA  entry;
        if ((dhandle_ = FindFirstFile((base_/"*").toString().c_str(), &entry)) != INVALID_HANDLE_VALUE) {
            started_ = true;
            if (isDotDir(entry.cFileName)) {
                nextEntry();
            } else {
                // I don't think the first entry is ever anything besides
                // "." so this is probably not reachable.
                // Still, this is how it would be written (see similar
                // code in nextEntry)
                filename_ = base_ / entry.cFileName;
                return haveFilename_ = true;
            }
        } else {
            throw_XSystem("FindFirstFile", base_.toString());
        }
    }
#endif
    return haveFilename_;
}

// This is a debug flag; this makes sure we don't copy when we shouldn't.
static bool prevent_dir_iterator_copy = false;

DirEntries::iterator::impl::impl(const impl &i)
    : base_(i.base_),
      filename_(i.filename_.getEncoding()),
      haveFilename_(i.haveFilename_),
      started_(i.started_)
{
    if(!started_ || !haveFilename_) {
        // Avoid UNINIT_CTOR
#ifndef _WIN32
        dhandle_ = NULL;
#else
        dhandle_ = NULL;
#endif
        return;
    }
    cond_assert(!prevent_dir_iterator_copy);
    // The MS documentation claims that one can reuse the directory handle,
    // but it doesn't quite work.
    //
    // seekdir() doesn't work on NetBSD 2 if pthread is linked in:
    //   http://cvsweb.de.netbsd.org/cgi-bin/cvsweb.cgi/src/lib/libc/gen/readdir.c
    //
    // So I do this manual seekdir/telldir below.
    //
    // CHG: We don't want to reuse the dir handle anyway, since it's
    // being used by another iterator already.
    // So instead we consider this is a new iterator, and iterate it
    // until we reach the same entry as is current in the other
    // iterator.
    started_ = false;
    haveFilename_ = true;
    do {
        if (!nextEntry()) {
            throw_XMsg("Unexpected change in directory contents");
        }
    } while (filename_ != i.filename_);
}

bool
DirEntries::iterator::impl::getNextEntryName(
    const char *&name,
    GetNextEntryArg &entry,
    GetNextEntryArgW &entryw
) {

#ifndef _WIN32
    errno = 0;
    dirent *ent = readdir(dhandle_);
    if(ent) {
        name = ent->d_name;
        return true;
    }
    if (errno == 0) {
        return false;
    } else {
        throw_XSystem("readdir");
    }
#else 
    if (base_.isUTF8Encoded()) {
        if(FindNextFileW(dhandle_, &entryw)) {
            entry_utf8 = ConsoleEncode::conv_wide_to_multi_byte_utf8_string(entryw.cFileName);
            name = entry_utf8.c_str();
            return true;
        }
        if (GetLastError() == ERROR_NO_MORE_FILES) {
            return false;
        } else {
            throw_XSystem("FindNextFileW");
        }
    } else {
        if(FindNextFile(dhandle_, &entry)) {
            name = entry.cFileName;
            return true;
        }
        if (GetLastError() == ERROR_NO_MORE_FILES) {
            return false;
        } else {
            throw_XSystem("FindNextFile");
        }
    }
#endif
}

bool
DirEntries::iterator::impl::nextEntry()
{
    // start the iteration if not started, and make sure this is not
    // an "end" iterator
    bool hasOneEntry = start();
    cond_assert(hasOneEntry);
    const char *name = NULL;

    GetNextEntryArg entry;
    GetNextEntryArgW entryw;

    // Get entries as long as there are any and they aren't "." or ".."
    do {
        if(!getNextEntryName(name, entry, entryw)) {
            // Finished the iteration.
            cleanup();
            return haveFilename_ = false;
        }
    } while(isDotDir(name));

    // Found a file!
#ifdef _WIN32
    filename_ = base_ / name;
#else
    if (base_.isUTF8Encoded()) {
        filename_ = base_ / Encoding::defaultEncodingToUTF8(name);
    } else {
        filename_ = base_ / name;
    }
#endif
    return haveFilename_ = true;
}


DirEntries::iterator::impl::~impl()
{
    if(!started_ || !haveFilename_)
        return;
    cleanup();
}

void DirEntries::iterator::impl::cleanup() {
#ifndef _WIN32
    if (closedir(dhandle_) != 0)
        throw_XSystem("closedir");
    dhandle_ = NULL;
#else
    if (FindClose(dhandle_) == 0)
        throw_XSystem("FindClose");
    dhandle_ = NULL;
#endif
}

bool
DirEntries::iterator::impl::hasEntry()
{
    return start();
}

const Filename&
DirEntries::iterator::impl::getEntry()
{
    start();
    return filename_;
}

DirEntries::iterator::iterator(const Filename &d)
    : impl_(new impl(d))
{
}

DirEntries::iterator::iterator()
    : impl_(NULL)
{
}

DirEntries::iterator::~iterator()
{
}

void DirEntries::iterator::checkAtEnd() const
{
    if(impl_.get()) {
        // If the iteration is *not* started, then copy-on-write and
        // then start (otherwise we may have to copy-on-write the
        // started version later, which is what we want to avoid)
        if(!impl_->started_ && !impl_.unique()) {
            impl_.reset(new impl(*impl_));
        }
        // Call start() even if "started_" already so that we can have
        // the value of hasEntry()
        if(!impl_->start()) {
            impl_.reset();
        }
    }
}

DirEntries::iterator&
DirEntries::iterator::operator++()
{
    string_assert(impl_.is_present(), "Attempted to increment end directory iterator");

    // Copy-on-write
    if(!impl_.unique()) {
        impl_.reset(new impl(*impl_));
    }
    if (!impl_->nextEntry()) {
        impl_.reset();
    }
    return *this;
}

const Filename&
DirEntries::iterator::operator*() const
{
    string_assert(impl_.is_present(), "Attempted to dereference end directory iterator");
    return impl_->getEntry();
}

bool
DirEntries::iterator::operator==(const iterator &other) const
{
    // Make sure "impl_" is unset if we're at "end()"
    checkAtEnd();
    other.checkAtEnd();
    return impl_ == other.impl_;
}

DirEntries::iterator::iterator(const iterator &other)
    : impl_(other.impl_)
{
}

DirEntries::iterator&
DirEntries::iterator::operator=(const iterator &other)
{
    impl_ = other.impl_;
    return *this;
}

DirEntries::DirEntries(const Filename &d)
    : dirname_(d)
{
}

DirEntries::iterator
DirEntries::begin() const
{
    return iterator(dirname_);
}

DirEntries::iterator
DirEntries::end() const
{
    return iterator();
}

// ------------------------- OrderedDirEntries ---------------------------

class OrderedDirEntries::iterator::impl {
    friend class OrderedDirEntries::iterator;
public:
    impl(const Filename &);
    impl(const impl &);
    ~impl();

    bool hasEntry() const;
    bool nextEntry();
    const Filename& getEntry() const;

private:
    refct_ptr< set<Filename> > entries_;
    set<Filename>::iterator    iterator_;
};

OrderedDirEntries::iterator::impl::impl(const Filename &f)
{
    DirEntries de(f);
    entries_ = new set<Filename>(de.begin(), de.end());
    iterator_ = entries_->begin();
}

OrderedDirEntries::iterator::impl::impl(const impl &i)
    : entries_(i.entries_), iterator_(i.iterator_)
{
}

OrderedDirEntries::iterator::impl::~impl()
{
}

bool
OrderedDirEntries::iterator::impl::hasEntry() const
{
    return iterator_ != entries_->end();
}

bool
OrderedDirEntries::iterator::impl::nextEntry()
{
    ++iterator_;
    return hasEntry();
}

const Filename&
OrderedDirEntries::iterator::impl::getEntry() const
{
    return *iterator_;
}

OrderedDirEntries::iterator::iterator(const Filename &d)
    : impl_(new impl(d))
{
    if (!impl_->hasEntry()) {
        impl_.reset();
    }
}

OrderedDirEntries::iterator::iterator()
    : impl_(NULL)
{
}

OrderedDirEntries::iterator::~iterator()
{
}

OrderedDirEntries::iterator&
OrderedDirEntries::iterator::operator++()
{
    string_assert(impl_.is_present(), "Attempted to increment end directory iterator");
    // Copy-on-write
    if(!impl_.unique()) {
        impl_.reset(new impl(*impl_));
    }
    if (!impl_->nextEntry()) {
        impl_.reset();
    }
    return *this;
}

const Filename&
OrderedDirEntries::iterator::operator*() const
{
    string_assert(impl_.is_present(), "Attempted to dereference end directory iterator");
    return impl_->getEntry();
}

bool
OrderedDirEntries::iterator::operator==(const iterator &other) const
{
    if (impl_ == other.impl_)
        return true;
    if (! impl_.is_present() || ! other.impl_.is_present())
        return false;
    return impl_->iterator_ == other.impl_->iterator_;
}

OrderedDirEntries::iterator::iterator(const iterator &other)
    : impl_(other.impl_)
{
}

OrderedDirEntries::iterator&
OrderedDirEntries::iterator::operator=(const iterator &other)
{
    impl_ = other.impl_;
    return *this;
}

OrderedDirEntries::OrderedDirEntries(const Filename &d)
    : dirname_(d)
{
}

OrderedDirEntries::iterator
OrderedDirEntries::begin() const
{
    return iterator(dirname_);
}

OrderedDirEntries::iterator
OrderedDirEntries::end() const
{
    return iterator();
}

pair<DirEntries::iterator, DirEntries::iterator>
RecursiveDirEntries::GetChildren::operator()(const Filename &dir) {
    pair<DirEntries::iterator, DirEntries::iterator> rv;
    rv.second = DirEntries::iterator();
    if(dir_exists(dir)) {
        rv.first = DirEntries::iterator(dir);
    } else {
        rv.first = rv.second;
    }
    return rv;
}

pair<OrderedDirEntries::iterator, OrderedDirEntries::iterator>
OrderedRecursiveDirEntries::GetChildren::operator()(const Filename &dir) {
    pair<OrderedDirEntries::iterator, OrderedDirEntries::iterator> rv;
    rv.second = OrderedDirEntries::iterator();
    if(dir_exists(dir)) {
        rv.first = OrderedDirEntries::iterator(dir);
    } else {
        rv.first = rv.second;
    }
    return rv;
}


void do_touch(const Filename &f) {
    if(!file_exists(f)) {
        eofstream of(f, eofstream::text);
        of << endl;
    }
}

CLOSE_NAMESPACE(FilenameNS)

MultiCaseFilenames::MultiCaseFilenames():
    fscNormalizedFileName(),
    casePreservedFileName()
{
}

MultiCaseFilenames::MultiCaseFilenames(
        const Filename &fscNormalizedFileName_,
        const Filename &casePreservedFileName_):
    fscNormalizedFileName(fscNormalizedFileName_),
    casePreservedFileName(casePreservedFileName_)
{
    string_assert(
        (fscNormalizedFileName.numNames()
         ==
         casePreservedFileName.numNames())
        &&
        (fscNormalizedFileName.isAbsolute()
         ==
         casePreservedFileName.isAbsolute()),
        "Both case-preserved and case-normalized must have the same shape");
}

bool MultiCaseFilenames::isEmpty() const
{
    return fscNormalizedFileName.isEmpty();
}

int MultiCaseFilenames::compare(const MultiCaseFilenames &other) const
{
    return fscNormalizedFileName.compare(other.fscNormalizedFileName);
}

MultiCaseFilenames MultiCaseFilenames::parents() const
{
    MultiCaseFilenames rv;
    rv.fscNormalizedFileName = fscNormalizedFileName.parent();
    rv.casePreservedFileName = casePreservedFileName.parent();
    return rv;
}

ostream &operator<<(ostream &out, const MultiCaseFilenames &m)
{
    return out << m.casePreservedFileName;
}


void efstreambuf::getStats(FileStats &buf) {
    flushWriteBuffer();
#ifdef __MC_MINGW__
    BY_HANDLE_FILE_INFORMATION fileInfo;
    if(!GetFileInformationByHandle(fd, &fileInfo)) {
        throw_XSystem("GetFileInformationByHandle");
    }
    buf.kind = FileStats::FK_REGULAR;
    buf.size = fileInfo.nFileSizeHigh;
    buf.size <<= 32;
    buf.size |= fileInfo.nFileSizeLow;
    buf.last_access = FilenameNS::FTIME_to_time_t(fileInfo.ftLastAccessTime);
    buf.last_modification = FilenameNS::FTIME_to_time_t(fileInfo.ftLastWriteTime);
#else
#  ifdef HAVE_STAT64
#define stat stat64
#define fstat fstat64
#endif
    struct stat sb;
    if(fstat(fd, &sb) != 0)
        throw_XSystem("fstat");
    stat_to_FileStats(sb, buf);
#undef stat
#undef fstat
#endif // !__MC_MINGW__
}


// ----------------------- test code -------------------------
using namespace FilenameNS;

// Return true if the two given comparison results are the same.
static bool sameComparison(int res1, int res2)
{
    if (res1 == res2) { return true; }
    if (res1 < 0 && res2 < 0) { return true; }
    if (res1 > 0 && res2 > 0) { return true; }

    return false;
}

static unsigned printFilenameTree(FilenameNode *n)
{
    unsigned count = 1;
    for (unsigned i=0; i<n->depth; i++) {
        cout << '\t';
    }
    cout << n->name << '\n';
    for (unsigned i=0; i<n->children.length(); i++) {
        count += printFilenameTree(n->children[i]);
    }
    return count;
}

// Basic tests that normalized strings come back unchanged.
//
// Also test the 'compare' routine.
static void testStringRoundtrips(SystemStringEncoding encoding)
{
    cout << "testStringRoundtrips" << endl;

    // names that are already normalized
    //
    // they are in sorted order according to Filename::compare
    char const * const names[] = {
        // Empty filesystem, absolute
        "/",
        "/foo",
        "/foo/z",

        // Empty filesystem, relative
        ".",
        "0",
        "A",
        "B",
        "Z",
        "a",
        "a/b",
        "a/foo",
        "a/foo/zoo",
        "foo",

        // Filesystem "C", absolute
        "C:/",

        // Filesystem "C", relative
        "C:blah2",

        // UNC path. '\' is between uppercase and lowercase.
        "\\\\unc/path",

        // Filesystem "c", absolute
        "c:/",
        "c:/program files/Coverity/blah",

        // Filesystem "c", relative
        "c:",
        "c:blah2",

    };

    long before = estimateFilenameMemoryUsage();

    Filename fnames[TABLESIZE(names)];

    // build them
    for (int i=0; i < TABLESIZE(names); i++) {
        fnames[i] = Filename(names[i], Filename::FI_PORTABLE, encoding);
        assert(fnames[i].getEncoding() == encoding);
        string s = fnames[i].toString();
        ostr_assert(s == string(names[i]),
                    names[i] << " turned into " << s);
        
        // check integrity
        estimateFilenameMemoryUsage();
    }

    // compare all pairs
    for (int i=0; i < TABLESIZE(names); i++) {
        for (int j=0; j < TABLESIZE(names); j++) {
            ostr_assert(
                sameComparison(fnames[i].compare(fnames[j]), compare(i,j)),
                "Expected " << fnames[i] << " to be to "
                << fnames[j] << " as " << i << " to " << j
            );
            ostr_assert(sameComparison(
                        fnames[i].compare(names[j],
                                          Filename::FI_PORTABLE,
                                          Filename::FCF_NONE),
                        compare(i,j)
                    ),
                "Expected " << fnames[i] << " to be to "
                << fnames[j] << " as " << i << " to " << j
            );
        }
    }

    // Sequence of pairs that should be equal with case-insensitive comparison
    char const *lcNames[] = {
        "A",
        "a",
        "C:foo",
        "c:Foo",
        "/Foo/Bar",
        "/foo/bar",
        "\\\\foo.Bar/file",
        "\\\\Foo.Bar/File",
    };
    for(int i = 0; i < ARRAY_SIZE(lcNames) / 2; ++i) {
        const char *n1 = lcNames[i * 2];
        const char *n2 = lcNames[i * 2 + 1];
        Filename f1(n1, Filename::FI_PORTABLE, encoding);
        Filename f2(n2, Filename::FI_PORTABLE, encoding);
        ostr_assert(
            !f1.compare(f2, Filename::FCF_CASE_INSENSITIVE),
            n1 << " should be equal to " << n2 << " (case-insensitive comparison)"
        );
        ostr_assert(
            !f1.compare(n2, Filename::FI_PORTABLE, Filename::FCF_CASE_INSENSITIVE),
            n1 << " should be equal to " << n2 << " (case-insensitive comparison to string)"
        );
    }

    cout << "used mem: " << (estimateFilenameMemoryUsage() - before) << endl;
}

template<Filename::FileComparisonFlag fcf>
static int compareFiles(const Filename &f1,
                        const Filename &f2)
{
    return f1.compare(f2, fcf);
}

static void printFile(ostream &out, Filename const &f)
{
    out << f;
}

// Test normalization process + comparison (especially comparison
// against unnormalized string)
static void testNormalization(SystemStringEncoding encoding)
{
    cout << "testNormalization" << endl;

    static struct S {
        char const *in;      // non-normalized
        char const *out;     // normalized
    } const arr[] = {
        { "a\\b",           "a/b" },
        { "a/",             "a" },
        { "/a//b//",      "/a/b" },
        { "/a/./b/././/", "/a/b" },
        { "/a/b/..",      "/a/b/.." },
        { "/a/b/a..",     "/a/b/a" },
        { "/a/b/a ",      "/a/b/a" },
        { "/a/b/a . . ",  "/a/b/a" },
        { "/a/b/..",      "/a/b/.." },
        { "/a/b/b.",      "/a/b/b" },
        { "/a/bb/bb",     "/a/bb/bb" },
        { "/a/bb/b",     "/a/bb/b" },
        { "/a/b/...",     "/a/b/..." },
        { "",               "." },
        { ".",               "." },
        { "./.",               "." },
        { "././/a/b/...",   "a/b/..." },
        // \\?\ prefix
        { "\\\\?\\C:\\file", "C:/file" },
        { "\\\\?\\UNC\\foo.Bar\\file", "\\\\foo.Bar/file" },
        { "//?/C:/file", "C:/file" },
        { "//?/UNC/foo.Bar/file", "\\\\foo.Bar/file" },
    };

    for (int i=0; i < TABLESIZE(arr); i++) {
        Filename f(arr[i].in, Filename::FI_WINDOWS, encoding);
        assert(f.getEncoding() == encoding);
        string s = f.toString();
        ostr_assert(s == string(arr[i].out),
                    "From " << arr[i].in << " expected " << arr[i].out
                    << " but got " << s);
        ostringstream ostr;
        ostr << f;
        cond_assert(ostr.str() == s);
    }
    // Test comparison; should be same with string version as with
    // non-string
    // This used to test more-or-less compatibility with strcmp, but
    // it's too approximate, and there's no much point in checking that.

    // Set of distinct files
    vector<Filename> files;
    // Set of file strings, to allow distinguishing the files.
    set<string> fileStrings;

    for(int i = 0; i < TABLESIZE(arr); i++) {
        Filename fi(arr[i].in, Filename::FI_WINDOWS, encoding);
        if(fileStrings.insert(arr[i].out).second) {
            files.push_back(fi);
        }
        for(int j = i; j < TABLESIZE(arr); j++) {
            Filename fj(arr[j].in, Filename::FI_WINDOWS, encoding);

            Filename::FileComparison res2;
            res2 = fi.compare(arr[j].in,
                              Filename::FI_WINDOWS,
                              Filename::FCF_NONE);

            // If the normalized strings are equal, then the files
            // must be equal, and vice-versa
            cond_assert(str_equal(arr[i].out, arr[j].out) == !res2);

            Filename::FileComparison res3;
            res3 = fi.compare(arr[j].out,
                              Filename::FI_WINDOWS,
                              Filename::FCF_NONE);
            cond_assert(res2 == res3);
            res3 = fi.compare(fj);
            cond_assert(res2 == res3);
            res3 = - fj.compare(arr[i].in,
                                Filename::FI_WINDOWS,
                                Filename::FCF_NONE);
            cond_assert(res2 == res3);
            res3 = - fj.compare(arr[i].out,
                                Filename::FI_WINDOWS,
                                Filename::FCF_NONE);
            cond_assert(res2 == res3);
            res3 = - fj.compare(fi);
            cond_assert(res2 == res3);
        }
    }
    // Test complete order properties
    check_comparator<Filename,
        int (*)(const Filename &, const Filename &)>
        (files,
         compareFiles<Filename::FCF_NONE>,
         printFile,
         /*allowEqual*/false);

    check_comparator<Filename,
        int (*)(const Filename &, const Filename &)>
        (files,
         compareFiles<Filename::FCF_CASE_INSENSITIVE>,
         printFile,
         /*allowEqual*/false);

    check_comparator<Filename,
        int (*)(const Filename &, const Filename &)>
        (files,
         compareFiles<Filename::FCF_FINAL_FIRST>,
         printFile,
         /*allowEqual*/false);
}

// Test that the parsing into components works.
static void testComponents(SystemStringEncoding encoding)
{
    cout << "testComponents" << endl;

    static struct S {
        // whole:
        char const *fname;   // string to parse

        // parts:
        char const *fs;      // filesystem, or NULL if none
        bool isAbsolute;     // true iff isAbsolute()
        char const *names;   // '/'-separated names or NULL if none
    } const arr[] = {
        { ".",                 NULL, false, NULL },
        { "a",                NULL, false, "a" },
        { "a/b",              NULL, false, "a/b" },
        { "a:/goo",           "a:", true,  "goo" },
        { "a:",               "a:", false, NULL },
        { "a:/",              "a:", true,  NULL },
        { "a:b/c/d",          "a:", false, "b/c/d" },
        { "/b/c/d",           NULL, true,  "b/c/d" },
        { "/",                NULL, true,  NULL },
        { "/x",               NULL, true,  "x" },
        { "\\\\foo.Bar/file", "\\\\foo.Bar", true, "file" },
    };

    for (int i=0; i < TABLESIZE(arr); i++) {
        S const &s = arr[i];
        Filename f1(s.fname, Filename::FI_WINDOWS, encoding);
        assert(f1.getEncoding() == encoding);

        // test round trip
        ostr_assert(str_equal(f1.toString(), s.fname),
                    "toString = " << f1.toString()
                    << " but expected " << s.fname);

        // check 's.fs'
        xassert(f1.hasFilesystem() == !!s.fs);
        if (f1.hasFilesystem()) {
            xassert(f1.getFilesystem() == string(s.fs));
        }

        // check 's.isAbsolute'
        xassert(f1.isAbsolute() == s.isAbsolute);

        // check 's.names'
        xassert(f1.hasNames() == !!s.names);
        if (f1.hasNames()) {
            // parse 's.names'
            StrtokParse components(s.names, "/");
            xassert(f1.numNames() == components.tokc());

            // confirm component-wise equality, working from
            // right to left in the name
            Filename p = f1;
            for (int i = components.tokc() - 1; i >= 0; i--) {
                xassert(p.finalName() == string(components[i]));
                p = p.parent();
            }
            xassert(p.isRoot());
        }

        // build a new filename by assembling the parts
        Filename f2 = Filename::getRoot(s.fs? s.fs : "", s.isAbsolute, encoding);
        if (s.names) {
            StrtokParse components(s.names, "/");
            for (int i=0; i < components; i++) {
                f2.appendSingleName(components[i]);
            }
        }
        assert(f2.getEncoding() == encoding);

        // should have ended up with the same thing
        xassert(f1 == f2);

        // check s.names using getRelative()
        xassert(f1.getRelative() ==
                Filename(s.names ? s.names : "", Filename::FI_WINDOWS, encoding));
        assert(f1.getRelative().getEncoding() == encoding);
    }
}

static void testComparison(SystemStringEncoding encoding)
{
    // Tests comparison
    struct FileCompData {
        const char *f1;
        const char *f2;
        Filename::FileComparison expectedComp;
        int expectedCommonPrefix;
        Filename::FileComparison expectedCompCaseInsensitive;
        int expectedCommonPrefixCaseInsensitive;
    };
    FileCompData tests[] = {
        {"a:", "A:", Filename::FC_GT, -1, Filename::FC_EQUAL, 0},
        {"A:", "A:", Filename::FC_EQUAL, 0, Filename::FC_EQUAL, 0},
        {"a:/b", "A:/b", Filename::FC_GT, -1, Filename::FC_EQUAL, 1},
        {"a:/", "a:/b", Filename::FC_PARENT, 0, Filename::FC_PARENT, 0},
        {"a:/b", "a:/b/c", Filename::FC_PARENT, 1, Filename::FC_PARENT, 1},
        {"a:/b", "a:/b", Filename::FC_EQUAL, 1, Filename::FC_EQUAL, 1},
        {"a:/b", "a:/B/c", Filename::FC_GT, 0, Filename::FC_PARENT, 1},
        {"a:/b/c", "a:/b/c/d", Filename::FC_PARENT, 2, Filename::FC_PARENT, 2},
        {"a:/b/c/e", "a:/b/c/d", Filename::FC_GT, 2, Filename::FC_GT, 2},
        {"b/c/e", "b/c/d", Filename::FC_GT, 2, Filename::FC_GT, 2},
        {"a:b/c", "b/c", Filename::FC_GT, -1, Filename::FC_GT, -1},
        {"a:/b/c", "b/c", Filename::FC_GT, -1, Filename::FC_GT, -1},
        {"/b/c", "b/c", Filename::FC_LT, -1, Filename::FC_LT, -1},
        {"a:/b/c", "/b/c", Filename::FC_GT, -1, Filename::FC_GT, -1},
        {"a:b/c", "/b/c", Filename::FC_GT, -1, Filename::FC_GT, -1},
        {"a:/b/c", "a:b/c", Filename::FC_LT, -1, Filename::FC_LT, -1}
    };
    for(int i = 0; i < ARRAY_SIZE(tests); ++i) {
        const FileCompData &t = tests[i];
        Filename f1(t.f1, Filename::FI_PORTABLE, encoding);
        Filename f2(t.f2, Filename::FI_PORTABLE, encoding);
        cond_assert(f1.compare(f2, Filename::FCF_NONE) == t.expectedComp);
        cond_assert(f2.compare(f1, Filename::FCF_NONE) == -t.expectedComp);
        cond_assert(f1.commonPrefixNumNames(f2, Filename::FCF_NONE) == t.expectedCommonPrefix);
        cond_assert(f2.commonPrefixNumNames(f1, Filename::FCF_NONE) == t.expectedCommonPrefix);
        cond_assert(f1.compare(f2, Filename::FCF_CASE_INSENSITIVE) == t.expectedCompCaseInsensitive);
        cond_assert(f2.compare(f1, Filename::FCF_CASE_INSENSITIVE) == -t.expectedCompCaseInsensitive);
        cond_assert(f1.commonPrefixNumNames(f2, Filename::FCF_CASE_INSENSITIVE) == t.expectedCommonPrefixCaseInsensitive);
        cond_assert(f2.commonPrefixNumNames(f1, Filename::FCF_CASE_INSENSITIVE) == t.expectedCommonPrefixCaseInsensitive);
    }
}

static void testFinalFirstComparison(SystemStringEncoding encoding)
{
    // Tests comparison
    struct FileCompData {
        const char *f1;
        const char *f2;
        Filename::FileComparison expectedComp;
        Filename::FileComparison expectedCompCaseInsensitive;
    };
    FileCompData tests[] = {
        {"b/c/e", "b/c/e", Filename::FC_EQUAL, Filename::FC_EQUAL},
        {"a/b", "b/a", Filename::FC_GT, Filename::FC_GT},
        {"a/a", "a/b", Filename::FC_LT, Filename::FC_LT},
        {"a/a", "b/a", Filename::FC_LT, Filename::FC_LT},
        {"a/b", "a/B", Filename::FC_GT, Filename::FC_EQUAL},
        {"a/b", "A/b", Filename::FC_GT, Filename::FC_EQUAL},
        {"a", "a", Filename::FC_EQUAL, Filename::FC_EQUAL},
        {"a", "b", Filename::FC_LT, Filename::FC_LT},
        {"b/c/d", "d/e", Filename::FC_LT, Filename::FC_LT},
        {"a/b/c", "b/c", Filename::FC_GT, Filename::FC_GT}
    };
    for(int i = 0; i < ARRAY_SIZE(tests); ++i) {
        const FileCompData &t = tests[i];
        Filename f1(t.f1, Filename::FI_PORTABLE, encoding);
        Filename f2(t.f2, Filename::FI_PORTABLE, encoding);
        cond_assert(f1.compare(f2, Filename::FCF_FINAL_FIRST) == t.expectedComp);
        cond_assert(f2.compare(f1, Filename::FCF_FINAL_FIRST) == -t.expectedComp);
        cond_assert(f1.compare(f2, Filename::FCF_CASE_INSENSITIVE | Filename::FCF_FINAL_FIRST) == t.expectedCompCaseInsensitive);
        cond_assert(f2.compare(f1, Filename::FCF_CASE_INSENSITIVE | Filename::FCF_FINAL_FIRST) == -t.expectedCompCaseInsensitive);
    }
}

static void testMakeRelative(SystemStringEncoding encoding)
{
    Filename f1("/A/b/C/d", Filename::FI_UNIX, encoding);
    Filename f2("/A/B", Filename::FI_UNIX, encoding);
    Filename f3("/A/b", Filename::FI_UNIX, encoding);
    Filename f4("/A/C", Filename::FI_UNIX, encoding);
    Filename f5("/d/e", Filename::FI_UNIX, encoding);

    // Use a base with a drive letter in all cases, because on Windows
    // it's required (See assert in "normalized").
    Filename base = Filename::getNamedRoot("A:", encoding);
    Filename rel = inner_make_relative(f1, f2, Filename::FCF_NONE, &base);
    assert_same_string(rel.toString(), "../b/C/d");
    rel = inner_make_relative(f1, f2, Filename::FCF_CASE_INSENSITIVE, &base);
    assert_same_string(rel.toString(), "C/d");
    rel = inner_make_relative(f1, f3, Filename::FCF_NONE, &base);
    assert_same_string(rel.toString(), "C/d");
    rel = inner_make_relative(f1, f4, Filename::FCF_NONE, &base);
    assert_same_string(rel.toString(), "../b/C/d");
    rel = inner_make_relative(f1, f5, Filename::FCF_NONE, &base);
    assert_same_string(rel.toString(), "A:/A/b/C/d");

    Filename fw1("A:/b/c/d", Filename::FI_PORTABLE, encoding);
    Filename fw2("A:/b/c", Filename::FI_PORTABLE, encoding);
    Filename fw3("A:/b/C", Filename::FI_PORTABLE, encoding);
    Filename fw4("a:/b/c", Filename::FI_PORTABLE, encoding);
    Filename fw5("B:/b/c", Filename::FI_PORTABLE, encoding);
    Filename fw6("A:/d/c", Filename::FI_PORTABLE, encoding);

    rel = inner_make_relative(fw1, fw2, Filename::FCF_CASE_INSENSITIVE, &base);
    assert_same_string(rel.toString(), "d");
    rel = inner_make_relative(fw1, fw3, Filename::FCF_CASE_INSENSITIVE, &base);
    assert_same_string(rel.toString(), "d");
    rel = inner_make_relative(fw1, fw4, Filename::FCF_CASE_INSENSITIVE, &base);
    assert_same_string(rel.toString(), "d");
    rel = inner_make_relative(fw1, fw5, Filename::FCF_CASE_INSENSITIVE, &base);
    assert_same_string(rel.toString(), "A:/b/c/d");
    rel = inner_make_relative(fw1, fw6, Filename::FCF_CASE_INSENSITIVE, &base);
    assert_same_string(rel.toString(), "A:/b/c/d");
}

// Test that filename validation works
static void testValidation(SystemStringEncoding encoding)
{
    cout << "testValidation" << endl;

    // trailing periods
    // DC: findNextComponent helpfully removes trailing periods when
    // constructing filenames for FI_WINDOWS, which I'm not sure is a
    // good idea considering that we otherwise don't bother to remap
    // illegal characters.
    cond_assert(!Filename("foo.", Filename::FI_UNIX, encoding).isValidForOS(Filename::FI_WINDOWS));
    cond_assert(!Filename("...", Filename::FI_UNIX, encoding).isValidForOS(Filename::FI_WINDOWS));
    cond_assert(Filename(".", Filename::FI_UNIX, encoding).isValidForOS(Filename::FI_WINDOWS));
    cond_assert(Filename("..", Filename::FI_UNIX, encoding).isValidForOS(Filename::FI_WINDOWS));

    const char *illegal[] = {
        // chars
        "C::/", "<stdout>", "*", "foo\x1f", "file\bname",
        // devices
        "con", "CON", "prn", "AuX", "AUX.exe", "nUL",
        "coM1", "COM2.com", "LPT9", "lpT8.bat",
        // really long name
        "C:/3456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789"
    };

    for (int i=0; i<TABLESIZE(illegal); i++) {
        Filename tmp1(illegal[i], Filename::FI_WINDOWS, encoding);
        cond_assert(!tmp1.isValidForOS(Filename::FI_WINDOWS));

        try {
            tmp1.throwIfInvalidForHost();
            cond_assert(!is_windows);
        } catch (...) {
            cond_assert(is_windows);
        }

        Filename tmp2(illegal[i], Filename::FI_UNIX, encoding);
        cond_assert(tmp2.isValidForOS(Filename::FI_UNIX));
    }

    const char *legal[] = {
        "C:", "C:/", "console", "compy86", "com123", "com", "com0", "com0.txt",
        // not so long name
        "C:/3456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "0123456789012345678901234567890123456789012345678/"
        "012345678/"
    };
    for (int i=0; i<TABLESIZE(legal); i++) {
        Filename tmp1(legal[i], Filename::FI_WINDOWS, encoding);
        cond_assert(tmp1.isValidForOS(Filename::FI_WINDOWS));
    }
}

// Exercise the root sharing mechanism and file name comparison.
static void testRootSharing(SystemStringEncoding encoding)
{
    cout << "testRootSharing" << endl;

    enum { 
        NUM = 1000,          // number of file names to make
        DIGITS = 3           // log base 10 of NUM
    };

    long before = estimateFilenameMemoryUsage();

    Filename fnames[NUM];

    for (int i=0; i < NUM; i++) {
        fnames[i] = Filename(encoding);
        // Injectively map 'i' into a filename that will compare
        // consistently with the integer source domain:
        //   000    ->    "a/a/a"
        //   001    ->    "a/a/b"
        //   999    ->    "j/j/j"
        int divisor = NUM / 10;
        for (int d=0; d < DIGITS; d++) {
            char n[2] = {
                'a' + (char)((i / divisor) % 10),
                '\0'
            };
            fnames[i].appendSingleName(n);
            divisor /= 10;
        }

        if (i == 12) {      // "012", except leading "0" would make it octal
            // check I didn't screw up the mapping
            xassert(fnames[i].toString() == string("a/b/c"));
        }
    }

    // quadratic comparison amongst all pairs
    for (int i=0; i<NUM; i++) {
        for (int j=0; j<NUM; j++) {
            xassert(sameComparison(fnames[i].compare(fnames[j]), compare(i,j)));
        }
    }

    cout << "used mem: " << (estimateFilenameMemoryUsage() - before) << endl;
                            
    Filename a("a", Filename::FI_UNIX, SSE_SYSTEM_DEFAULT);
    FilenameNS::trimFilenameMemoryUsage(&a);

    cout << "used mem after trim \"a\": " << (estimateFilenameMemoryUsage() - before) << endl;

    FilenameNS::trimFilenameMemoryUsage();

    cout << "used mem after trim all: " << (estimateFilenameMemoryUsage() - before) << endl;
}

// Test calls to "normalize()"
static void testNormalize(SystemStringEncoding encoding)
{
    cout << "testNormalization" << endl;
    if(false) {
        // Just use this function to avoid a warning (it's only used
        // on Windows otherwise)
        const char *n;
        int l;
        getFinalName(n, l, Filename::FI_HOST);
    }

    static struct S {
        char const *in;      // non-normalized
        char const *out;     // normalized
        char const *outl;    // normalized + lowercased
    } const arr[] = {
        { "a",              "C:/Base/a", "c:/base/a" },
        { ".",              "C:/Base",   "c:/base" },
        { "..",             "C:/",       "c:/" },
        { "D:/",            "D:/",       "d:/" },
        { "a/b/..",         "C:/Base/a", "c:/base/a" },
        // we're not testing the case where the drive letter is different; it
        // would require creating directories to modify Windows's cwd state.
        { "C:foo",          "C:/Base/foo", "c:/base/foo" },
        { "a/b/../c",       "C:/Base/a/c", "c:/base/a/c" },
        { "C:/a/b/../c",    "C:/a/c",      "c:/a/c" },
        { "C:/a/../..",     "C:/",         "c:/" },
        { "\\\\foo.Bar/file",     "\\\\foo.Bar/file",         "\\\\foo.bar/file" },
        { "//foo.Bar/..",     "\\\\foo.Bar/",         "\\\\foo.bar/" },
        { "/a/b/../c",      "C:/a/c",      "c:/a/c" },
        { "D:/a/Bar/fO",    "D:/a/Bar/fO", "d:/a/bar/fo" },
        { "c:/Foo",         "c:/Foo",      "c:/foo"} ,
    };

    // Use a drive letter as there's an assertion, on Windows, that it
    // is present. It also should not cause problems on other filesystems.
    Filename base = Filename::getNamedRoot("C:", encoding) / "Base";
    // Base, lowercased. "normalized" doesn't normalize the base.
    Filename lcBase = base.normalized(Filename::NF_LOWERCASE_ALWAYS, /*base*/ NULL);
    
    for (int i=0; i < TABLESIZE(arr); i++) {
        Filename f(arr[i].in, Filename::FI_PORTABLE, encoding);
        Filename fl = f.normalized(Filename::NF_LOWERCASE_ALWAYS, &lcBase);
        f = f.normalized(Filename::NF_NONE, &base);
        string s = f.toString();
        ostr_assert(s == string(arr[i].out),
                    "From " << arr[i].in << " expected " << arr[i].out
                    << " but got " << s);
        s = fl.toString();
        ostr_assert(s == string(arr[i].outl),
                    "From " << arr[i].in << " expected " << arr[i].outl
                    << " but got " << s);
        Filename f2 = Filename::normalizedFile
            (arr[i].in,
             Filename::FI_PORTABLE,
             Filename::NF_NONE,
             &base,
             encoding);
        ostr_assert(f == f2,
                    "From " << arr[i].in << " expected " << arr[i].out
                    << " but got " << f2);
        Filename fl2 = Filename::normalizedFile
            (arr[i].in,
             Filename::FI_PORTABLE,
             Filename::NF_LOWERCASE_ALWAYS,
             &lcBase,
             encoding);
        ostr_assert(fl == fl2,
                    "From " << arr[i].in << " expected " << arr[i].out
                    << " but got " << fl2);
    }

    // Test symlinks (on everything besides Windows)
#ifndef __MC_MINGW__
    {
        bool changed;
        Filename testDir = Filename::getRelativeRoot(encoding) / "testNormalize";

        // normalize the base directory to limit later normalization to
        // everything below that.
        testDir = testDir.normalized(Filename::NF_NONE, /*base*/ NULL, changed);

        create_directories(testDir);
        create_directories(testDir / "aaa");
        create_directories(testDir / "bbb");
        symlink("../aaa",
                (testDir / "bbb " / "ccc").toString().c_str());

        Filename orig = testDir / "bbb" / "ccc" / ".." / "foo.h";
        Filename normalized = orig.normalized(Filename::NF_RESOLVE_SYMLINKS, /*base*/ NULL, changed);
        Filename expected = testDir / "foo.h";

        ostr_assert(normalized == expected,
                    "From " << orig << " expected " << expected
                    << " but got " << normalized);

        remove_all(testDir);
    }
#endif
}

static void testFileCase(SystemStringEncoding encoding)
{
    // This test only works on case-insensitive filesystems.
    if(!is_windows) {
        return;
    }
    cout << "testFileCase" << endl;
    Filename testCaseDir = Filename::getRelativeRoot(encoding) / "testCaseDir";
    create_directories(testCaseDir);
    Filename testCaseDir2 = Filename::getRelativeRoot(encoding) / "TeStCaSeDiR";
    Filename testCaseFile = testCaseDir / "MixedCase";
    do_touch(testCaseFile);
    Filename testCaseFile2 = testCaseDir2 / "MiXedCaSe";
    Filename testCaseFileNotHere = testCaseDir / "MixedCaseNotHere";
    Filename testCaseFile3 = testCaseFile2.normalized(Filename::NF_NORMALIZE_CASE, /*base*/ NULL);
    cout << "Normalized file name: " << testCaseFile3 << endl;
    cond_assert(str_equal(testCaseFile3.finalName(), testCaseFile.finalName()));
    cond_assert(str_equal(testCaseFile3.parent().finalName(), testCaseDir.finalName()));
    Filename testCaseFileNotHere2 = testCaseFileNotHere.normalized(Filename::NF_NORMALIZE_CASE, /*base*/ NULL);
cout << "Normalized file name: " << testCaseFileNotHere2 << endl;
    cond_assert(str_equal(testCaseFileNotHere.finalName(), testCaseFileNotHere2.finalName()));
    cond_assert(str_equal(testCaseFileNotHere.parent().finalName(), testCaseDir.finalName()));
    remove_all(testCaseDir);
}

static void testPreserveRelative() {
    cout << "testPreserveRelative" << endl;
    const char *relative_filename="a/b/../c";
    Filename relative_f(relative_filename,Filename::FI_UNIX);
    Filename nf5 = relative_f.normalized(Filename::NF_NORMALIZE_CASE|Filename::NF_PRESERVE_RELATIVE,NULL);
    string nf5_str=nf5.toString();
    string relative_f_str=relative_f.toString();
    cond_assert(nf5_str.size()-relative_f_str.size()>0);
    cond_assert(nf5_str.substr(nf5_str.size()-relative_f_str.size()) == relative_f_str);
}

static void testFilenameClass(SystemStringEncoding encoding)
{
    testStringRoundtrips(encoding);
    testNormalization(encoding);
    testComponents(encoding);
    testComparison(encoding);
    testFinalFirstComparison(encoding);
    testMakeRelative(encoding);
    testValidation(encoding);
    testRootSharing(encoding);
    testNormalize(encoding);
    testFileCase(encoding);

    // Test Filename's operator <<
    xassert(string("a/b/c") == stringb(Filename("a/b/c", Filename::FI_MAGIC, encoding)));
}

static void testOperations(SystemStringEncoding encoding)
{
    // We want to make sure we don't copy started iterators in this code
    prevent_dir_iterator_copy = true;
    Filename test_dir = Filename::getRelativeRoot(encoding) / "test-dir";
    if(dir_exists(test_dir)) {
        throw_XUserError("I want to create test-dir but it's already there");
    }
    create_directory(test_dir);
    cond_assert(dir_exists(test_dir));
    remove(test_dir);
    cond_assert(!dir_exists(test_dir));
    Filename test_subdir = test_dir / "test-subdir";
    create_directories(test_subdir);
    cond_assert(dir_exists(test_subdir));
    Filename test_subdir2 = test_dir / "test-subdir2";
    create_directories(test_subdir2);
    cond_assert(dir_exists(test_subdir2));
    Filename test_file1 = test_dir / "file1";
    Filename test_file2 = test_dir / "xfile2";
    Filename test_file3 = test_subdir / "file3";
    do_touch(test_file1);
    do_touch(test_file2);
    do_touch(test_file3);
    // We'll do an ordered non-recursive dir iteration
    Filename dir_iteration_files[] = {
        test_file1,
        test_subdir,
        test_subdir2,
        test_file2
    };
    Filename rec_dir_iteration_files[] = {
        test_file1,
        test_subdir,
        test_file3,
        test_subdir2,
        test_file2
    };
    int n = 0;
    foreach(i, ordered_dir_entries(test_dir)) {
        ostr_assert(*i == dir_iteration_files[n],
                    "Got " << *i << " but expected " << dir_iteration_files[n]);
        ostr_assert((*i).getEncoding() == encoding, "Got unexpected encoding in " << dir_iteration_files[n]);
        ++n;
    }
    cond_assert(n == ARRAY_SIZE(dir_iteration_files));
    n = 0;
    foreach(i, ordered_recursive_dir_entries(test_dir)) {
        cond_assert(*i == rec_dir_iteration_files[n]);
        cond_assert((*i).getEncoding() == encoding);
        ++n;
    }
    cond_assert(n == ARRAY_SIZE(rec_dir_iteration_files));

    // Try the special case where the "begin()" iterator is also
    // "end()"
    // This is important because we delay computing that until
    // comparison.
    {
        DirEntries::iterator b(test_subdir2);
        DirEntries::iterator b2(b);
        cond_assert(b == DirEntries::iterator());
        cond_assert(b2 == DirEntries::iterator());
        cond_assert(b2 == b);
    }


    prevent_dir_iterator_copy = false;
    OrderedDirEntries::iterator i = ordered_dir_entries(test_dir).begin();
    n = 0;
    for(; n < 2; ++n) {
        cond_assert(*i == dir_iteration_files[n]);
        ++i;
    }
    OrderedDirEntries::iterator j = i;
    int m = n;
    for(; i != OrderedDirEntries::iterator(); ++i) {
        cond_assert(*i == dir_iteration_files[n]);
        ++n;
    }
    cond_assert(n == ARRAY_SIZE(dir_iteration_files));
    for(; j != OrderedDirEntries::iterator(); ++j) {
        cond_assert(*j == dir_iteration_files[m]);
        ++m;
    }
    cond_assert(m == ARRAY_SIZE(dir_iteration_files));

    unsigned expected = ARRAY_SIZE(rec_dir_iteration_files) + 1 /*test_dir*/;
#ifndef __MC_MINGW__
    cond_assert(symlink("/fakefile", (test_dir / "mylink").toString().c_str()) == 0);
    expected++;
#endif

    cond_assert(!atomic_rename(test_file1, test_file2));
#ifdef __MC_MINGW__
    try {
        rename(test_file1, test_file2);
        // On windows, MoveFileEx() fails if the destination file
        // already exists. The above statement is expected to throw
        // an XMsg exception. The following statement should never
        // be reached unless the expected behavior isn't met.
        cond_assert(false);
    } catch(XMsg& e) {
        if(encoding == SSE_SYSTEM_DEFAULT) {
            cond_assert(has_substring(string(e.what()), "MoveFileExA"));
        } else if(encoding == SSE_UTF8){
            cond_assert(has_substring(string(e.what()), "MoveFileExW"));
        } else {
            cond_assert(false);
        }
    }
#endif
    {
        // Maximum difference between "now" as returned by time() and
        // the file system time after creating or writing a file, in
        // seconds.  On hardware with normal load, we would expect the
        // difference to be well under one second.  However, as the
        // saga of bug 36056 shows, build machines are often under
        // very high load, which can unpredictably delay processes for
        // a long time.  Also, some file systems (FAT) have a fairly
        // coarse time stamp granularity (2s), so we need to be above
        // that to avoid a race even on unloaded hardware.
        int const threshold_seconds = 60;

        // Test set_last_write_time.
        // Fails on nonexistent files.
        Filename nonexistent = test_subdir / "nonexistent";
        cond_assert(!file_exists(nonexistent));
        cond_assert(!set_last_write_time_now(nonexistent));
        // get_last_write_time returns 0 on nonexistent files.
        cond_assert(get_last_write_time(nonexistent) == 0);

        // writing the file sets its mod time to now
        time_t modTime = get_last_write_time(test_file3);
        time_t now = time(NULL);
        cond_assert(abs(now - modTime) <= threshold_seconds);

        // setting the mod time causes the set time to be retained
        static const time_t specialTime = 123456789;
        cond_assert(set_last_write_time(test_file3, specialTime));
        cond_assert(specialTime == get_last_write_time(test_file3));

        // setting mod time to now sets it to a time matching system clock
        cond_assert(set_last_write_time_now(test_file3));
        modTime = get_last_write_time(test_file3);
        now = time(NULL);
        cond_assert(abs(now - modTime) <= threshold_seconds);

        set_file_read_only(test_file3);
        if (is_windows) {
            // setting time on read-only file fails
            cond_assert(!set_last_write_time(test_file3, specialTime));
        }
        else {
            // setting time on read-only file succeeds
            cond_assert(set_last_write_time(test_file3, specialTime));
            // and the new time is set
            cond_assert(specialTime == get_last_write_time(test_file3));
        }
        set_file_writable(test_file3);
    }

    cond_assert(remove_all(test_dir) == expected);
    cond_assert(!dir_exists(test_dir));
}

void file_operations_unit_tests()
{
    testOperations(SSE_SYSTEM_DEFAULT);
    testOperations(SSE_UTF8);
}

void filename_class_unit_tests(int argc, char const * const *argv)
{
    Filename::clearNormalizeCache();
    Filename::clearSymlinkCache();
    // Allocate the file name that contains the initial path; it's
    // used in testFileCase()
    Filename::initialPath();
    const unsigned oldSize = printFilenameTree(FilenameNS::getSuperRootNode());

    if(argc > 1) {
        // In this case, assume this is a file name to normalize
        // The next argument, if any, is a FilenameInterp (as an int)
        // The next one, if any is a set of NormalizeFlags, in binary.
        // The next one is a "base" for normalization
        Filename::NormalizeFlags flags = Filename::NF_NONE;
        Filename::FilenameInterp interp = Filename::FI_HOST;
        if(argc >= 3) {
            const char *s = argv[2];
            interp = (Filename::FilenameInterp)string_to_int(s);
        }
        if(argc >= 4) {
            const char *s = argv[3];
            for(int i = 0; s[i]; ++i) {
                if(s[i] == '1') {
                    flags = (Filename::NormalizeFlags)(flags | (1 << i));
                }
            }
        }
        Filename base(SSE_SYSTEM_DEFAULT);
        Filename *b = 0;
        if(argc >= 5) {
            base = Filename(argv[4], Filename::FI_HOST, SSE_SYSTEM_DEFAULT);
            b = &base;
        }
        const char *filename = argv[1];
        cout << "File argument: " << filename << endl;
        Filename f(filename, interp, SSE_SYSTEM_DEFAULT);
        cout << "File after parsing: " << f << endl;
        Filename nf1 = f.normalized(flags, b);
        cout << "File after normalization: " << nf1 << endl;
        Filename nf2 = f.normalized(flags, b);
        cout << "File after normalization, again (check cache): " << nf2 << endl;
        cond_assert(nf1 == nf2);
        Filename nf3 = Filename::normalizedFile(filename, interp, flags, b);
        cout << "normalizedFile: " << nf3 << endl;
        cond_assert(nf1 == nf3);
        Filename nf4 = Filename::normalizedFile(filename, interp, flags, b);
        cout << "normalizedFile, again (check cache): " << nf4 << endl;
        cond_assert(nf1 == nf4);
                    
        return;
    }
    
    cout << "used mem at start: " << estimateFilenameMemoryUsage() << endl;

    testFilenameClass(SSE_SYSTEM_DEFAULT);
    testFilenameClass(SSE_UTF8);
    testPreserveRelative();

    cout << "used mem at end: " << estimateFilenameMemoryUsage() << endl;

    Filename::clearNormalizeCache();
    Filename::clearSymlinkCache();
    const unsigned newSize = printFilenameTree(FilenameNS::getSuperRootNode());

    // Confirm that everything from this unit test is freed.  Some other
    // Filename instances may have been allocated before this test was run.
    xassert(oldSize == newSize);
}

void gdb_print_filename(const Filename &f) {
    cout << f << endl;
}


// EOF
