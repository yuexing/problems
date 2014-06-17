// filename-class.hpp
// class Filename, for efficiently storing many file names in memory
// (c) 2008-2014 Coverity, Inc. All rights reserved worldwide.

// Some interface ideas are borrowed from boost::path.

// Conceptually, a file name has three parts:
//
//   filesystem root: optional name of a filesystem, e.g. a
//     Windows drive letter
//   absolute marker: optional marker indicating name is absolute
//   sequence of names: possibly empty sequence of names, from
//     the name of the uppermost directory down to the final
//     component of the file name in question
//
// Names lacking an absolute marker are "relative" names, but are not
// interpreted as being relative to anything in particular.  So, for
// example, if I create "a/b" and then chdir to "a", the created file
// name does not magically change to "b"; it is still "a/b".
//
// The file name with all components missing is written "." and denotes
// the "root" of all relative file names.
//
// For now, this module tries to interpret file names consistently
// across platforms, without regard to the nature of the host OS.
// When file names are written, they are written using "/" as the
// separator.
//
// Also for now, this module is entirely case-sensitive, including for
// the filesystem specifiers (and therefore drive letters).  I'm still
// not sure what the best way to deal with case insensitivity is.
//
// Finally, note that this module is *not*, by itself, thread-safe.
// It uses a shared tree of file name components behind the scenes.

#ifndef FILENAME_CLASS_HPP
#define FILENAME_CLASS_HPP

#include "istring/istring.hpp"       // istring
#include "macros.hpp"                // OPEN_NAMESPACE
#include "compiler-extensions.hpp"   // DEPRECATED
#include "bit-masks-alt.hpp"         // ENUM_BITWISE_OR
#include "system/is_windows.hpp"     // is_windows
#include "text/string-fwd.hpp"       // string
#include "text/iostream-fwd.hpp"     // ostream
#include "containers/set-fwd.hpp"    // set
#include "flatten/flatten-fwd.hpp"   // Flatten
#include "char-enc/encoding-fwd.hpp" // Encoding
#include "sized-types.hpp"           // uint32

#include "filename-class-fwd.hpp"    // fwd for this file
#include "path-fwd.hpp"              // path

#include <stddef.h>                  // NULL
#include <stdio.h>                   // FILE

OPEN_NAMESPACE(FilenameNS)

// For a hack which represents fully qualified names in C# and Java as files.
const char *getFQNFileSystem();

// Opaque implementation.  See filename-class-impl.hpp.
class FilenameNode;

class DirEntries;
class OrderedDirEntries;

typedef uint32 FilenameNodeIndex;

// Store a single immutable file name.
class Filename {
public:
    // Different ways to interpret a string as a path
    enum FilenameInterp {
        // Unix-style: '/' is separator, anything else is part of a name
        FI_UNIX,
        // Same as unix style, except <letter>: at the beginning
        // is considered a drive letter. Use e.g. for a file coming
        // from cov-emit that was later stringized but is known to be absolute.
        FI_UNIX_WITH_DRIVE_LETTER,
        // An alias for the above (since this is the format we use to
        // serialize/deserialize filenames; also the format we use to
        // print filenames.
        FI_PORTABLE = FI_UNIX_WITH_DRIVE_LETTER,
        // Windows-style: '/' and '\' are separators
        // Starting with <letter>: indicates a drive letter
        // a "." at the end of a name is ignored
        FI_WINDOWS,
        // Attempt to guess what to use. Right now, that means
        // FI_WINDOWS (since it is necessary on Windows, and on Unix
        // should give reasonable results)
        FI_MAGIC = FI_WINDOWS,
        // FI_WINDOWS on windows, FI_UNIX on Unix
        FI_HOST = is_windows ? FI_WINDOWS : FI_UNIX,
    };

    // Needed to ensure we don't implicitly construct with boost::path
    enum FromBoostPath { FROM_BOOST_PATH };

    // Modifier flags for "normalize" below.
    enum NormalizeFlags {
        NF_NONE                  = 0,
        // Use long file name (Windows only, no effect on Unix)
        NF_LONGNAME              = 1 << 0,
        // Use short file name (Windows only, no effect on Unix)
        NF_SHORTNAME             = 1 << 1,
        // Lowercase all components. Note that if you do that on a
        // case-sensitive filesystem, resolving symlinks probably
        // won't work.
        NF_LOWERCASE_ALWAYS      = 1 << 2,
        // Resolve all symlinks. Note that <symlink>/.. is always resolved
        // automatically, as not doing so would be incorrect.
        NF_RESOLVE_SYMLINKS      = 1 << 3,
        // Normalize the case of all components by querying the
        // filesystem (Windows only; no effect on Unix since file
        // systems are case sensitive)
        NF_NORMALIZE_CASE        = 1 << 4,
        // Ignore ".." when normalizing
        NF_PRESERVE_RELATIVE     = 1 << 5,
        // Lowercase all components (Windows only)
        NF_LOWERCASE_IF_WINDOWS  = is_windows ? NF_LOWERCASE_ALWAYS : 0,
    };

    enum FileComparison {
        FC_LT     = -2,
        FC_PARENT = -1,
        FC_EQUAL  =  0,
        FC_CHILD  =  1,
        FC_GT     =  2
    };

    // Comparison flags
    enum FileComparisonFlag {
        // Nothing special.
        FCF_NONE                         = 0,
        // Case-insensitive
        FCF_CASE_INSENSITIVE             = 1 << 0,
        // Case-insensitive, on Windows only
        FCF_CASE_INSENSITIVE_IF_WINDOWS  =
        is_windows ? FCF_CASE_INSENSITIVE : 0,
        // Alternative comparison check, comparing final names
        // and using the rest of the path (from the end) to
        // deterministically resolve duplicates.
        FCF_FINAL_FIRST                  = 1 << 1
    };

    // Parse a filename with the given interpretation.
    // if the filename is absolute, "fn" is first called with
    // [name;name + length) representing the filesystem, and "isStart"
    // set to true.
    // isAbsolute indicates whether the file is going to be relative
    // or not (and is only set for this first call)
    // filesystem (e.g. drive letter); may be empty.
    // Then, fn is called for each component of the
    // filename. Component is [name;name + length].
    // If fn returns "false" at any point, parsing stops.
    // Return value indicates whether parsing was stopped.
    static bool parseFilename
    (const char *name,
     bool (*fn)(const char *name,
                int length,
                bool isStart,
                bool isAbsolute,
                void *arg),
     void *arg,
     FilenameInterp interp);
    
    friend class FilenameNode;
    friend class FilenameBuilder;

private:     // data
    // Index of node in the tree that identifies this file.
    // Need to maintain reference count.
    FilenameNodeIndex node_idx : 31;

    // Indicates what encoding the file name should be interpreted as
    // (UTF-8, system default, etc).
    SystemStringEncoding encoding : SYSTEM_STRING_ENCODING_BITS;

private:      // funcs
    // Wrap an existing node; does *not* bump the refct.
    explicit Filename(FilenameNode *node, SystemStringEncoding encoding);

    FilenameNode * getNode() const;

    // Append names using a node.
    void appendNamesFromNode(FilenameNode *node);
                                
    // Allow adding a name w/o going all the way to the NUL.
    // If the name turns out to be ".", this is a no-op.
    void appendSingleNameWithLen(char const *name, int length);

    // Called by ctors that take strings
    void init(const char*name, FilenameInterp fn);

    // hasChanged indicates whether the return value is different from "this"
    Filename normalizedWhenComplete(NormalizeFlags flags,
                                    bool &hasChanged) const;

    // Append a single name, while normalizing.
    // Assuming "this" is a normalized file name, the result is still
    // a normalized file name.
    // If "withAppended" is not null, it must be equal to the result
    // of appending "name" to "this". If it is given, and the result
    // of the operation makes "this" equal to "withAppended",
    // this function will return "false".
    bool normalizedAppendSingleNameWithLen
    (NormalizeFlags flags,
     char const *name,
     int length,
     const Filename *withAppended);

    // Function to implement toString and toUnicodeFileString
    string toStringInternal(FilenameInterp fi, bool unicodePrefix) const;

    FileComparison compareInternal(
        Filename const &obj,
        FileComparisonFlag flags,
        int */*nullable*/commonPrefixSize) const;

public:      // funcs
    // ---- basic lifecycle ----
    // Create a filename from a string.  
    //
    // If the string begins with "X:" where X is an uppercase or
    // lowercase letter, treat it as denoting a drive letter.
    //
    // After the optional drive letter, a leading "/" or "\" is
    // interpreted as indicating an absolute root.
    //
    // After that, any occurrence of "/" or "\" is treated as a name
    // separator.  The use of "/" vs. "\" need not be consistent in
    // the name.  As implied by the conceptual schema, the Filename
    // object does not remember which separators were used.
    //
    // Consecutive separators are treated as a single separator,
    // rather than creating empty-string name components.  (But such
    // components *can* be created using 'appendSingleName'.)  A
    // trailing separator is similarly ignored.
    //
    // The interpretation is FI_MAGIC (see below)
    //    explicit Filename(char const *name);
    //    explicit Filename(rostring name);

    // Create a filename, using the given interpretation
    Filename(char const *name, FilenameInterp fn, SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);
    Filename(rostring name, FilenameInterp fn, SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);
    DEPRECATED Filename(const path &p, FromBoostPath dummy, SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT); // temporarily implicit

    // Convert to Boost path; useful for global variables that were converted
    // from 'path' to 'Filename'
    DEPRECATED operator path() const;

    // Creates as ".", the relative root.
    explicit Filename(SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);

    // copy
    Filename(Filename const &obj);
    Filename& operator= (Filename const &obj);

    // destroy
    ~Filename();

    SystemStringEncoding getEncoding() const { return encoding; }
    bool isUTF8Encoded() const { return encoding == SSE_UTF8; }

    // ---- validation ----
    // This indicates whether the file name is valid for the given
    // interpretation.
    // Currently only FI_WINDOWS can have invalid file names.
    bool isValidForOS(FilenameInterp fn) const;

    // This indicates whether the file name is valid for the host OS
    bool isValidForHost() const { return isValidForOS(FI_HOST); }

    // This will throw an XUserError with message
    // "Invalid file name: <file>" if the file name is invalid.
    void throwIfInvalidForHost() const;

    // ---- creating roots ----
    // Return a root filename, upon which other filenames may be
    // built.  The 'names' portion of the filename is empty.
    //
    // If 'fs' is "", this is an anonymous (unix-like) root, otherwise
    // it is a named root.  For a Windows-like drive letter, the colon
    // *must* be included in the 'fs' argument; it is not implicit.
    //
    // If 'absolute' is true, this file name includes the absolute
    // marker.
    static Filename getRoot(char const *fs, bool absolute, SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);

    // Return a filename for "/", the root of a unix-like
    // filesystem without drive letters.
    static Filename getAbsoluteRoot(SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT)
        { return getRoot("", true /*absolute*/, encoding); }

    // Return a filename for the root of a windows-like filesystem
    // rooted with a drive letter, e.g., "c:/".
    static Filename getNamedRoot(char const *fs, SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT)
        { return getRoot(fs, true /*absolute*/, encoding); }

    // Return a filename for "", the root of relative names.
    static Filename getRelativeRoot(SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT)
        { return getRoot("", false /*absolute*/, encoding); }

    // Return a filename for the root of a windows-like filesystem
    // rooted with a drive letter, e.g., "c:".
    static Filename getNamedRelativeRoot(char const *fs, SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT)
        { return getRoot(fs, false /*absolute*/, encoding); }
        
        
    // ---- appending names ----
    // Append a single name to an existing file name, and return
    // the new name (the original is not modified).  Even if 'name'
    // contains "/" or "\" characters, treat it as a single name
    // component.
    Filename withAppendedSingleName(char const *name) const;

    // Copy 'this' and append all of the names in 'names'.  The
    // filesystem and absolute marker, if any, are *ignored*.  If
    // there are no name components in 'name', returns an object
    // equal to 'this'.
    Filename withAppendedNames(Filename const &names) const;

    // Equivalent to "withAppendedNames(Filename(names))".
    Filename withAppendedNames(char const *names) const;
    Filename withAppendedNames(rostring names) const;

    // In-place modifying versions of the above.  For example:
    //   fn.appendNames(names);
    // is equivalent to:
    //   fn = fn.withAppendedNames(names);
    // Returns '*this'.
    Filename& appendSingleName(char const *name);
    Filename& appendSingleName(rostring name);
    Filename& appendNames(Filename const &names);
    Filename& appendNames(char const *names);
    Filename& appendNames(rostring names);
    Filename& appendNames(char const *names, FilenameInterp fn);
    Filename& appendNames(rostring names, FilenameInterp fn);

    Filename operator/ (Filename const &names) const
        { return withAppendedNames(names); }
    Filename operator/ (char const *names) const
        { return withAppendedNames(names); }
    Filename operator/ (rostring names) const
        { return withAppendedNames(names); }

    Filename& operator/= (Filename const &names)
        { return appendNames(names); }
    Filename& operator/= (char const *names)
        { return appendNames(names); }
    Filename& operator/= (rostring names)
        { return appendNames(names); }


    /**
     * Normalize the filename.
     *
     * A normalized filename is an absolute filename with redundant ('.')
     * directories removed and parent directories ('..') resolved.
     *
     * The filename is assumed to be either absolute or relative to
     * "base".
     *
     * If base is null, the "&initialPath()" will be used instead
     * (computing current directory every time could be expensive,
     * this can be called often and needs to be fast)
     * If a base is given, it will *not* be normalized. If it's not
     * given, initialPath() will be normalized with the given flags.
     * This is for performance, to avoid pointlessly normalizing the
     * base every time.
     * Special case:
     * On Windows, if the filename is relative with a drive letter, we'll
     * try to figure out what the current directory is on that drive,
     * and use that (whether "base" is given or not). This should be
     * very uncommon.
     * Uses an internal cache.
     * Can throw an XUserError if it calls resolveAllSymlinks (which
     * it may, even without NF_RESOLVE_SYMLINKS, to resolve a "..")
     * Indicates in "hasChanged" whether the returned value is
     * different from "this"
     **/
    Filename normalized
    (NormalizeFlags flags,
     const Filename *base,
     bool &hasChanged) const;

    Filename normalized
    (NormalizeFlags flags,
     const Filename *base) const;

    // Given a string representing a file, return the normalized
    // version.
    // This is equivalent to
    // Filename(f, interp).normalize(flags, base) but more efficient
    // if "f" is relative.
    // No "hasChanged" since filename interpretation can often cause a
    // change, which there's currently no way to record.
    static Filename normalizedFile
    (const char *f,
     FilenameInterp interp,
     NormalizeFlags flags,
     const Filename *base,
     SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);

    // Returns *this if isAbsolute(), otherwise base / *this
    Filename completed(const Filename &base);

    // Clears the cache used by "normalize" above
    static void clearNormalizeCache();
    // Clears the cache used by "resolveSymlink" below
    // Also clears caches for resolveLongName and resolveShortName
    static void clearSymlinkCache();

    // If this file is a symlink, replace it with the symlink target (only go 1 level)
    // Return whether the file actually was a symlink.
    bool resolveSymlink();

    // Replace the last element of the path with its "long name"
    // version
    // Returns whether it was different
    // Only relevant on Windows
    bool resolveLongName();

    // Replace the last element of the path with its "long name" version
    // Returns whether it was different
    // Only relevant on Windows
    bool resolveShortName();

    // Either of the above, depending on the flag.
    bool resolveLongOrShortName(bool shortName);

    // Follows the chain of symlinks until the actual file.
    // Throws an XUserError if there are too many levels (>32
    // currently), like an errno ELOOP.
    // TODO: Check sysconf instead of arbitrarily using 32
    // Returns whether at least 1 symlink was resolved.
    bool resolveAllSymlinks();
    
    // Current directory, not normalized in any way.
    static Filename currentPath(SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);

    // Current directory at the time this function is first called.
    // Not normalized in any way.
    static const Filename &initialPath(SystemStringEncoding encoding = SSE_SYSTEM_DEFAULT);
    
    // ---- deconstructing ----
    // Return true iff the filename specifies a filesystem.
    bool hasFilesystem() const;

    // Return the specified filesystem.  Returns the empty string
    // if 'hasFilesystem()' is false.
    const char *getFilesystem() const;

    // Return true iff the filename is absolute (carries the absolute
    // marker).
    bool isAbsolute() const;

    // A clone of the boost function, makes the filename not start with whatever
    // signifies the root directory on this system. For instance, C:\foo\bar
    // becomes foo\bar.
    Filename getRelative() const;

    // Return true iff the filename contains at least one 'name'
    // component.
    bool hasNames() const;

    // Being a root is the opposite of having names.    
    bool isRoot() const { return !hasNames(); }

    // The empty file (relative root, ".")
    bool isEmpty() const {
        return !hasNames() && !isAbsolute() && !hasFilesystem();
    }
    // "isEmpty" is a better name for a predicate, but most people use "empty"
    bool empty() const { return isEmpty(); }
    
    // Return the number of name components present.
    int numNames() const;
    
    // Return the final name component.  Requires 'hasNames()'.
    const char *finalName() const;

    // Return a filename referring to the parent of 'this'.  Requires
    // 'hasNames()'.
    Filename parent() const;

    // ---- comparison ----
    // Return:
    //   FC_PARENT if 'this' is a parent of obj (i.e. calling
    // obj.parent() enough times would yield an object equal to
    // "this"). This is considered "lower".
    //   FC_LT if "less" for some other reason (e.g. different
    //   components)
    //   FC_EQUAL if 'this' equals 'obj'
    //   FC_CHILD if 'obj' is a parent of 'this'
    //   FC_GT if "greater" for some other reason
    //
    // The order is lexicographic:
    //   1. Filesystem specifier; "" is less than all.
    //   2. Absolute marker; absolute is less than relative.
    //   3. Name components, in order.  If one name sequence is
    //      a proper prefix of the other, the prefix is less.
    //
    // When comparing strings (filesystem or name component), the
    // standard library 'strcmp' order is used (strcasecmp if doing
    // case-insensitive comparison)
    //
    // If 'flags' has FCF_FINAL_FIRST set the name components are
    // compared in reverse order (i.e. starting at the final name).
    // Name components will be compared until:
    //   1. There is a difference (FC_LT or FC_GT will be returned).
    //   2. We run out of name components in one or both filenames
    //      (the filename with fewer name components will be 'FC_LT'
    //      than the other). If both have the same number of names
    //      they will be FC_EQUAL.
    //   3. No special processing is done for filesystem specifiers
    //      or absolute markers. The intent of this flag is to
    //      order by final name, with the other name components
    //      being taken into account to resolve duplicates in a
    //      deterministic way.
    // Note that this flag can only result in FC_LT, FC_EQUAL or
    // FC_GT being returned, i.e. no attempt is made to resolve
    // parents.
    FileComparison compare(Filename const &obj, FileComparisonFlag flags = FCF_NONE) const;
    
    // Equivalent to "compare(Filename(str))".
    FileComparison compare(char const *str) const;
    
    // Equivalent to "compare(Filename(str, fn), fc)".
    // It is an error to call this with the FCF_FINAL_FIRST flag.
    FileComparison compare(char const *str, FilenameInterp fn, FileComparisonFlag fc) const;

    // Return the number of components in the common prefix between
    // this and "obj" (if any), comparing each component according to
    // the flags.
    // If the "filesystem" part is different, this will return -1.
    // It is an error to call this with the FCF_FINAL_FIRST flag.
    int commonPrefixNumNames(Filename const &obj, FileComparisonFlag flags = FCF_NONE) const;
    
    // returns !compare(other, fc) || compare(other, fc) == FC_PREFIX
    // It is an error to call this with the FCF_FINAL_FIRST flag.
    bool isParent(const Filename &other,
                  FileComparisonFlag fc) const;
    
    // Operators for convenience.  The operators acting upon char*
    // should be safe because Filename has no implicit conversion
    // operators.
    #define MAKE_OP(op)                                \
        bool operator op (Filename const &obj) const   \
            { return compare(obj) op 0; }              \
        bool operator op (char const *str) const       \
            { return compare(str) op 0; }
    MAKE_OP(==)
    MAKE_OP(!=)
    MAKE_OP(<=)
    MAKE_OP(>=)
    MAKE_OP(<)
    MAKE_OP(>)
    #undef MAKE_OP

    size_t compute_hash(FileComparisonFlag flags = FCF_NONE) const;
    size_t compute_hash(
        FileComparisonFlag flags,
        size_t h
    ) const;

    // Comparator for "set" and "map" where you can specify flags
    template<FileComparisonFlag fcf> struct lt_t {
        bool operator()(const Filename &f1, const Filename &f2) const {
            return f1.compare(f2, fcf) < 0;
        }
    };

    // Same as above, except that the comparison flags can change at
    // run-time.
    struct lt_var_flag_t {
        lt_var_flag_t(FileComparisonFlag fcf): fcf(fcf) {}

        bool operator()(const Filename &f1, const Filename &f2) const {
            return f1.compare(f2, fcf) < 0;
        }

        FileComparisonFlag const fcf;
    };
    
    // ---- stringification ----
    // Return a string in the following form:
    //
    //   [filesystem] sep? [name1 [ sep [name2 ... ] ] ]
    //
    // where the leading sep corresponds to the absolute marker.  The
    // separator sep is a backslash (\) when fi==FI_WINDOWS, otherwise it is
    // a forward slash (/). Returned string will either be formated in UTF-8 or
    // the system default, depending on the FileEncoding flag associated with
    // the file name.
    string toString(FilenameInterp fi = FI_PORTABLE) const;

    // Equivalent to toString(Filename::FI_HOST)
    string toHostStyleString() const;

    // Generate a string with the Windows "long name" prefix (\\?\)
    // This first normalizes the file.
    // See http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx
    // This then needs to be converted to wide character before
    // passing to Windows unicode APIs.
    // The expectation is that the file name is in UTF-8; Windows uses
    // UTF-16.
    string toWindowsStringWithLongNamePrefix() const;

    // Generate a string that is in the system default encoding. If the
    // file name is marked as UTF-8 encoded it will be converted. This is
    // primarily used for passing to non-Windows OS APIs.s
    string toSystemDefaultString() const;

    // Generate a UTF-8 string. If the file name is represented using the
    // system default encoding, it will be converted in computing the
    // returned string.
    string toUTF8String() const;

    // Write a string in the FI_PORTABLE format to 'os', and return 'os'.
    // XXX: We *should* use FI_HOST format, but some code relies on using
    // FI_PORTABLE...
    ostream& write(ostream &os) const;

    // Same, but write to a FILE, with XML escaping.
    void writeToFILE_xmlEscape(FILE *dest) const;

    // xfer to Flatten; defined in flatten/flatutil.cpp for dependency reasons.
    void xferFields(Flatten &flat);
};

DECLARE_ISTRING_INSERTER(Filename const &);

inline Filename::FileComparison operator-(Filename::FileComparison f1) {
    return (Filename::FileComparison)-(int)f1;
}

inline ostream& operator<< (ostream &os, Filename const &obj)
    { return obj.write(os); }

// Return an estimate of the number of bytes used by the Filename
// component tree.  Also do self-checks; if something is wrong,
// throw XSelfCheck.
long estimateFilenameMemoryUsage();

// Attempt to trim any excess memory used by child arrays in the name
// tree.  If 'subtree' is not NULL, limit trimming to the subtree
// rooted at 'subtree'.
void trimFilenameMemoryUsage(Filename const *subtree = NULL);

ENUM_BITWISE_OR(Filename::NormalizeFlags);

ENUM_BITWISE_OR(Filename::FileComparisonFlag);

void xferGeneric(Flatten &flat, Filename &file);

// Calls toWindowsStringWithLongNamePrefix and turns the result into
// Unicode using the default code page.
// Ideally this would go into the i18n library but this would cause
// recursive dependency issues.
// This can only be called on Windows.
wstring toWindowsStringWithLongNamePrefixUnicode(const Filename &f);

#ifdef __MC_MINGW__
// Give a Windows error code (e.g. from GetLastError), return an
// "errno" value.
// Returns -1 if no corresponding errno code is known.
// "errcode" should be a DWORD, but including windows.h is
// prohibitively annoying (see similar problem with HANDLE in
// efstream.hpp)
int getErrnoFromWindowsError(uint32 errcode);
#endif

CLOSE_NAMESPACE(FilenameNS)

// Struct containing both a case-normalized and a case-preserved file
// name.
// Mostly useful for our source front ends, but this is a fairly
// convenient place to put this.
class MultiCaseFilenames {
public:
    MultiCaseFilenames();
    MultiCaseFilenames(
        const Filename &fscNormalizedFileName_,
        const Filename &casePreservedFileName_);

    // Return a MultiCaseFilenames with the parent of each file.
    MultiCaseFilenames parents() const;

    // Compare against another file. Will only compare the
    // case-normalized version.
    int compare(const MultiCaseFilenames &other) const;

    COMPARISON_OPERATOR_METHODS(MultiCaseFilenames);

    bool isEmpty() const;

public:
    // The case-normalized file name.
    Filename fscNormalizedFileName;
    // The case-preserved file name.
    Filename casePreservedFileName;
};

// This will print the case-preserved version.
ostream &operator<<(ostream &out, const MultiCaseFilenames &);

void filename_class_unit_tests(int argc, char const * const *argv);

#endif // FILENAME_CLASS_HPP
