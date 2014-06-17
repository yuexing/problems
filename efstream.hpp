/**
 * \file efstream.hpp
 * "Enhanced" fstream ('e' stands for "exception" because it will
 * throw on error)
 *
 * (c) 2010-2011,2013 Coverity, Inc. All rights reserved worldwide.
 **/
#ifndef COV_EFSTREAM_HPP
#define COV_EFSTREAM_HPP

#include "libs/bit-masks-alt.hpp" // ENUM_BITWISE_OPS
#include "libs/containers/vector.hpp" // vector
#include "libs/macros.hpp" // override
#include "libs/text/iostream.hpp"
#include "libs/text/fstream.hpp"

// We can't include cov-windows.hpp here, because this header is used
// too often, and windows.h has a lot of symbols that clash with our
// own.
// Instead, we'll use "void *" directly instead of HANDLE

// #include "libs/system/cov-windows.hpp" // HANDLE
#include "libs/system/is_windows.hpp" // is_windows

#include "filename-class.hpp"
#include "path-fwd.hpp"

// I used to use OF_ as prefix, but Windows already has OF_READ
// and OF_WRITE (for the win16 OpenFile)
// This should also make the enumerators unique enough that I can put
// them in the global namespace.
enum OpenFlags {
    // Open for reading
    OPENFLAGS_READ            = 1 << 0,
    // Open for writing
    // One of OPENFLAGS_READ or OPENFLAGS_WRITE must be set.
    OPENFLAGS_WRITE           = 1 << 1,
    // Read & write.
    OPENFLAGS_RW     = (OPENFLAGS_READ | OPENFLAGS_WRITE),
    // Allow creating the file if it doesn't exist
    // Always considered unset without OPENFLAGS_WRITE
    OPENFLAGS_ALLOW_CREATE    = 1 << 2,
    // Allow opening an existing file.
    // Always considered set without OPENFLAGS_ALLOW_CREATE
    OPENFLAGS_ALLOW_EXISTING  = 1 << 3,
    // Allow subprocesses to inherit the handle.
    // Without this, on Unix files are FD_CLOEXEC
    OPENFLAGS_INHERIT         = 1 << 4,
    // Truncate file if it exists (combine with OPENFLAGS_ALLOW_EXISTING)
    // Only use with OPENFLAGS_WRITE
    OPENFLAGS_TRUNCATE        = 1 << 5,
    // Open for appending
    // Only use with OPENFLAGS_WRITE
    OPENFLAGS_APPEND          = 1 << 6,
    // Convert a written lf into a cr lf
    // Convert a read cf lf into a lf
    OPENFLAGS_CONVERT_CRLF    = 1 << 7,
    // Text mode, as done by libc (it's like OPENFLAGS_CONVERT_CRLF on
    // Windows, no-op on other platforms)
    OPENFLAGS_TEXT            = 1 << 8,
    // Open in binary mode, i.e. not text.
    // One of TEXT or BINARY must be set, on penalty of assertion
    // failure.
    // The idea is to avoid doing the wrong thing on Linux and then
    // get weird results on Windows.
    OPENFLAGS_BINARY          = 1 << 9,
    // On Windows, this causes use of the Unicode API + the '\\?\'
    // prefix to extend the path length limit.
    // This is not done by default because this involves extra
    // processing, which cause cause errors; furthermore, many Windows
    // utilities (including Windows explorer) can't handle these files
    // so it may be preferable to fail rather than create these kinds
    // of files.
    OPENFLAGS_ALLOW_LONG_PATH = 1 << 10,
    // Delete the file when closed or the process exits.
    // Used for temporary files which must be deleted regardless of
    // how the process is exited. Note that on *nix the file will
    // be created/opened in the file system then immediately unlinked.
    // On Windows, the file will remain visible in the file system
    // until deleted (closed or process exit).
    OPENFLAGS_DELETE_ON_CLOSE = 1 << 11,
    OPENFLAGS_ALL             = (1 << 12) - 1
};

// Is it binary or text?
// Having a default is a bad idea; neither default is good.
enum TextModeFlag {
    TMF_TEXT,
    TMF_BINARY
};

// Like filebuf, but named efstreambuf for naming consistency
class efstreambuf:
    public streambuf {
public:
#ifdef __MC_MINGW__
    typedef void *file_descriptor_t;
#else
    typedef int file_descriptor_t;
#endif

    // I'm not providing ctors that open, because they could throw and
    // that's annoying.
    // The iostreams will provide these ctors; throwing is not as much
    // of a problem in them because they don't have buffers to free
    efstreambuf();

    // Open the file.
    // Throws an XSystem on failure.
    // This doesn't throw a file_open_error because these contain
    // boost::path, and I don't want that.
    // If you want to open a file that may not exist, check
    // file_exists first. This also avoids throwing exceptions in
    // normal operations, which makes debugging easier.
    void open(const Filename &f, OpenFlags flags);
    void open(const path &f, OpenFlags flags);
    void open(const char *f, OpenFlags flags);
    void open(const string &f, OpenFlags flags);
    // Close the file, if open.
    void close();
    // Close the file if open; may throw if !uncaught_exception()
    ~efstreambuf();

    // Obtain file stats.
    // On Windows, stats will be based on the original file name; on
    // Unix, will use fstat.
    // On Unix, it's also implemented in filename-class.cpp along with
    // getFileStats.
    // This calls flushWriteBuffer() so that "size" will be correct.
    void getStats(FileStats &stats);
    // Indicates whether the file is currently open.
    bool isOpen() const;
    // Name of the last open file.
    // Should only be called if is_open()
    Filename getFilename() const;
    // Obtain the underlying file descriptor.
    file_descriptor_t getFileDescriptor() const;

    // Change the size of the internal buffer.
    // Only use this for testing.
    // The size needs to be at least 2.
    // This can only be done on an open file (otherwise it will
    // assert; it is actually pointless to resize an unopen file
    // because opening will resize the buffer)
    void resizeBuffer(size_t newSize);
    
    // streambuf overrides
protected:

    int sync();
    
    // Seeking
    override std::streampos seekoff
    (std::streamoff off,
     std::ios_base::seekdir way,
     std::ios_base::openmode which);
    override std::streampos seekpos
    (std::streampos sp,
     std::ios_base::openmode which);

    // Output
    override traits_type::int_type overflow(traits_type::int_type ch);
    override streamsize xsputn(const traits_type::char_type* s, streamsize n);

    // Input
    override streamsize showmanyc();
    override streamsize xsgetn (traits_type::char_type* s, streamsize n);
    override traits_type::int_type underflow();
    // I'm not sure what the point of re-implementing "uflow" would
    // be, so don't.
    
    // TODO: implement pbackfail
    // I don't think we ever use the iostream API that calls that, so
    // this can wait.

private:
    char *buf_begin();
    char *buf_end();
    size_t buf_size() const;
    // Non-virtual "sync"
    void flushWriteBuffer();
    bool convertCRLF() const;
    // File name; empty iff not open.
    Filename file;
    // Internal buffer
    // Can be a read buffer or a write buffer depending on the last
    // operation.
    // The internal buffer pointers (gptr(), pptr()) may point to it
    // or be NULL; only one of them will be non-NULL at a time.
    // NULL iff not open
    vector<traits_type::char_type> buf;
    // File descriptor
    file_descriptor_t fd;
    // Flags used to open the file
    OpenFlags flags;
    // Indicates that we have a trailing CR in CRLF conversion mode.
    // It may be suppressed if reading a LF later.
    bool trailing_cr;
};

ENUM_BITWISE_OPS(OpenFlags, OPENFLAGS_ALL);

class eifstream: public istream {
public:
    eifstream();

    // Shortcuts for text mode flags.
    static const TextModeFlag text = TMF_TEXT;
    static const TextModeFlag binary = TMF_BINARY;

    // Open with default flags, in text or binary mode.
    eifstream
    (const Filename &,
     TextModeFlag);
    // Open with the given flags + OPENFLAGS_READ
    eifstream
    (const Filename &,
     OpenFlags flags);
    eifstream
    (const path &,
     TextModeFlag);
    eifstream
    (const path &,
     OpenFlags);
    eifstream
    (const char *,
     TextModeFlag);
    eifstream
    (const char *,
     OpenFlags);
    eifstream
    (const string &,
     TextModeFlag);
    eifstream
    (const string &,
     OpenFlags);


    void open
    (const Filename &,
     TextModeFlag);
    void open
    (const Filename &,
     OpenFlags);
    void open
    (const path &,
     TextModeFlag);
    void open
    (const path &,
     OpenFlags);
    void open
    (const char *,
     TextModeFlag);
    void open
    (const char *,
     OpenFlags);
    void open
    (const string &,
     TextModeFlag);
    void open
    (const string &,
     OpenFlags);

    void close();
    bool is_open() const;
    

    // Default read flags.
    // It's simply OPENFLAGS_READ
    static const OpenFlags readFlags;
    efstreambuf buf;
};

class eofstream: public ostream {
public:
    eofstream();

    // Shortcuts for text mode flags.
    static const TextModeFlag text = TMF_TEXT;
    static const TextModeFlag binary = TMF_BINARY;

    // Open and truncate, in text or binary mode.
    // If e.g. "ios::app" would be required, the version with
    // OpenFlags should be used instead, with appendFlags |
    // OPENFLAGS_BINARY/TEXT as argument.
    explicit eofstream
    (const Filename &p,
     TextModeFlag);
    // OPENFLAGS_WRITE will be added to the open flags
    explicit eofstream
    (const Filename &p,
     OpenFlags);
    explicit eofstream
    (const path &p,
     TextModeFlag);
    explicit eofstream
    (const path &p,
     OpenFlags);
    explicit eofstream
    (const string &p,
     TextModeFlag);
    explicit eofstream
    (const string &p,
     OpenFlags);
    explicit eofstream
    (const char *p,
     TextModeFlag);
    explicit eofstream
    (const char *p,
     OpenFlags);

    void open
    (const path &p,
     TextModeFlag);
    void open
    (const path &p,
     OpenFlags);
    void open
    (const Filename &p,
     TextModeFlag);
    void open
    (const Filename &p,
     OpenFlags);
    void open
    (const string &p,
     TextModeFlag);
    void open
    (const string &p,
     OpenFlags);
    void open
    (const char *p,
     TextModeFlag);
    void open
    (const char *p,
     OpenFlags);

    void close();
    bool is_open() const;
    
    // Flags to open for appending (allowing creation)
    // That's what's used for functions taking a TextModeFlag (then
    // OPENFLAGS_TEXT or OPENFLAGS_BINARY is added)
    // It's OPENFLAGS_WRITE | OPENFLAGS_ALLOW_EXISTING | OPENFLAGS_ALLOW_CREATE | OPENFLAGS_TRUNCATE
    static const OpenFlags truncateFlags;
    // Flags to open for appending (allowing creation)
    // Corresponds to an ios::app.
    // OPENFLAGS_TEXT/OPENFLAGS_BINARY should be added to it.
    // It's OPENFLAGS_WRITE | OPENFLAGS_ALLOW_EXISTING | OPENFLAGS_ALLOW_CREATE | OPENFLAGS_APPEND
    static const OpenFlags appendFlags;
    efstreambuf buf;
};

class efstream: public istream, public ostream {
public:
    efstream();

    // Shortcuts for text mode flags.
    static const TextModeFlag text = TMF_TEXT;
    static const TextModeFlag binary = TMF_BINARY;

    // Open and truncate, in text or binary mode.
    // If e.g. "ios::app" would be required, the version with
    // OpenFlags should be used instead, with appendFlags |
    // OPENFLAGS_BINARY/TEXT as argument.
    explicit efstream
    (const Filename &p,
     TextModeFlag);
    // OPENFLAGS_WRITE will be added to the open flags
    explicit efstream
    (const Filename &p,
     OpenFlags);
    explicit efstream
    (const path &p,
     TextModeFlag);
    explicit efstream
    (const path &p,
     OpenFlags);
    explicit efstream
    (const string &p,
     TextModeFlag);
    explicit efstream
    (const string &p,
     OpenFlags);
    explicit efstream
    (const char *p,
     TextModeFlag);
    explicit efstream
    (const char *p,
     OpenFlags);

    void open
    (const path &p,
     TextModeFlag);
    void open
    (const path &p,
     OpenFlags);
    void open
    (const Filename &p,
     TextModeFlag);
    void open
    (const Filename &p,
     OpenFlags);
    void open
    (const string &p,
     TextModeFlag);
    void open
    (const string &p,
     OpenFlags);
    void open
    (const char *p,
     TextModeFlag);
    void open
    (const char *p,
     OpenFlags);

    void close();
    bool is_open() const;

    // Flags to open for appending (allowing creation)
    // That's what's used for functions taking a TextModeFlag (then
    // OPENFLAGS_TEXT or OPENFLAGS_BINARY is added)
    // It's OPENFLAGS_WRITE | OPENFLAGS_ALLOW_EXISTING | OPENFLAGS_ALLOW_CREATE | OPENFLAGS_TRUNCATE
    static const OpenFlags truncateFlags;
    // Flags to open for appending (allowing creation)
    // Corresponds to an ios::app.
    // OPENFLAGS_TEXT/OPENFLAGS_BINARY should be added to it.
    // It's OPENFLAGS_WRITE | OPENFLAGS_ALLOW_EXISTING | OPENFLAGS_ALLOW_CREATE | OPENFLAGS_APPEND
    static const OpenFlags appendFlags;

    // Flags to open for a new temporary file which will be deleted on close
    // or process exit.
    // OPENFLAGS_TEXT/OPENFLAGS_BINARY should be added to it.
    // It's OPENFLAGS_WRITE | OPENFLAGS_READ | OPENFLAGS_ALLOW_CREATE
    //   | OPENFLAGS_TRUNCATE | OPENFLAGS_DELETE_ON_CLOSE
    static const OpenFlags temporaryFile;
    efstreambuf buf;
};

void efstream_unit_tests(int argc, const char * const *argv);

// An ofstream that uses libc (unlike "eofstream" above), so that it can
// appropriately set the stdin/stdout/stderr integer file descriptors.
// Only useful on Windows, because on Unix eofstream does use integer
// file descriptors.
struct intfd_ofstream:
    public ofstream {
    intfd_ofstream();
    void open(const Filename &f,
              std::ios_base::openmode openmode);
};

#endif // COV_EFSTREAM_HPP
