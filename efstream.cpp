/**
 * \file efstream.cpp
 * "Enhanced" fstream ('e' stands for "exception" because it will
 * throw on error)
 *
 * (c) 2010-2014 Coverity, Inc. All rights reserved worldwide.
 **/

#include "efstream.hpp"
#include "filename-class.hpp"
#include "filename.hpp"
#include "xfile.hpp"                   // file_does_not_exist_error
#include "path.hpp"

#include "libs/exceptions/assert.hpp"            // cond_assert, ostr_assert
#include "libs/exceptions/user-error.hpp"        // throw_XUserError
#include "libs/exceptions/xsystem.hpp"           // throw_XSystem
#include "libs/text/stream-utils.hpp"            // copy_stream
#include "libs/text/string-utils.hpp"            // string_to_int, str_equal
#include "libs/text/parse.hpp"                   // escapeCString

#ifndef __MC_MINGW__
#include <unistd.h> // open, read, write
#include <fcntl.h>  // fcntl
#include <errno.h>  // errno
#include <sys/stat.h>  // S_IWUSR
#endif
#include "libs/system/cov-windows.hpp"
#include "file/file-operations.hpp"              // FileStats

#include <string.h>                    // memcpy, memchr

const TextModeFlag eifstream::text;
const TextModeFlag eifstream::binary;
const TextModeFlag eofstream::text;
const TextModeFlag eofstream::binary;


efstreambuf::efstreambuf():
    file(SSE_SYSTEM_DEFAULT),
    buf(),
    fd(),
    flags(),
    trailing_cr(false)
{
}

void efstreambuf::open(const char *f,
                       OpenFlags flags) {
    open(
        Filename(
            f,
            Filename::FI_HOST,
            SSE_SYSTEM_DEFAULT
        ), flags
    );
}

void efstreambuf::open(const string &f,
                       OpenFlags flags) {
    open(f.c_str(), flags);
}

void efstreambuf::open(const path &f, OpenFlags flags) {
    open(Filename(f.native_file_string(),
                  Filename::FI_HOST,
                  SSE_SYSTEM_DEFAULT), flags);
}


// True if debugging is enabled for this module.
static bool efstreamDebug()
{
    static bool ret = (getenv("COVERITY_EFSTREAM_DEBUG") != NULL);
    return ret;
}


// Throw an error when trying to open directory "f" as a file.
static void throwDirError(const Filename &f) {
    throw_XUserError(stringb("Cannot open \"" << f <<
                             "\" because it is a directory"));
}

void efstreambuf::open(const Filename &f, OpenFlags flags) {
    if(flags & OPENFLAGS_TEXT) {
        if(is_windows) {
            flags |= OPENFLAGS_CONVERT_CRLF;
        }
        ostr_assert(
            !(flags & OPENFLAGS_BINARY),
            f << " was opened in both binary and text mode"
        );
    } else {
        ostr_assert(
            flags & OPENFLAGS_BINARY,
            f << " was opened in neither binary nor text mode"
        );
    }
    size_t buf_size = 0;
    // Close if previously open
    close();
    this->file = f;
    this->flags = flags;
    if(!(flags & OPENFLAGS_WRITE)) {
        flags &= ~OPENFLAGS_ALLOW_CREATE;
    }
    if(!(flags & OPENFLAGS_ALLOW_CREATE)) {
        flags |= OPENFLAGS_ALLOW_EXISTING;
    }
#ifdef __MC_MINGW__
    SECURITY_ATTRIBUTES attr;
    attr.nLength = sizeof(attr);
    attr.lpSecurityDescriptor = NULL;
    attr.bInheritHandle =
        (flags & OPENFLAGS_INHERIT) ? TRUE : FALSE;
    DWORD dwDesiredAccess = 0;
    if(flags & OPENFLAGS_READ) {
        dwDesiredAccess |= GENERIC_READ;
    }
    if(flags & OPENFLAGS_WRITE) {
        dwDesiredAccess |= GENERIC_WRITE;
    }
    DWORD dwCreationDisposition;
    if(flags & OPENFLAGS_ALLOW_CREATE) {
        if(flags & OPENFLAGS_ALLOW_EXISTING) {
            if(flags & OPENFLAGS_TRUNCATE) {
                dwCreationDisposition = CREATE_ALWAYS;
            } else {
                dwCreationDisposition = OPEN_ALWAYS;
            }
        } else {
            dwCreationDisposition = CREATE_NEW;
        }
    } else {
        if(flags & OPENFLAGS_TRUNCATE) {
            dwCreationDisposition = TRUNCATE_EXISTING;
        } else {
            dwCreationDisposition = OPEN_EXISTING;
        }
    }
    DWORD dwFlagsAndAttributes;
    if (flags & OPENFLAGS_DELETE_ON_CLOSE) {
        dwFlagsAndAttributes = FILE_FLAG_DELETE_ON_CLOSE;
    } else {
        dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    }

    // Try to open the file.  Because Windows is awesome, we might
    // need to try more than once.  See bug 62701.
    for (int attempts = 0; true; ) {
        attempts++;

        if(flags & OPENFLAGS_ALLOW_LONG_PATH || f.getEncoding() == SSE_UTF8) {
            fd = CreateFileW(
                toWindowsStringWithLongNamePrefixUnicode(f).c_str(),
                dwDesiredAccess,
                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                &attr,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                /*hTemplateFile*/NULL
            );
        } else {
            fd = CreateFileA(
                f.toString().c_str(),
                dwDesiredAccess,
                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                &attr,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                /*hTemplateFile*/NULL
            );
        }

        // The attempt limit is 30 because in my testing I have
        // seen it fail three times in a row with a 200 ms delay,
        // so I am setting the limit at 10x what I've seen.
        if (fd == INVALID_HANDLE_VALUE &&
            attempts < 30 &&
            GetLastError() == ERROR_USER_MAPPED_FILE)
        {
            // gcov-begin-ignore
            // The Windows kernel sometimes spuriously reports this error.
            // Try again.
            if (efstreamDebug()) {
                cerr << "efstreambuf::open(\"" << f << "\", " << flags
                     << "): CreateFile failed with ERROR_USER_MAPPED_FILE, trying again; "
                     << "attempts=" << attempts << endl;
            }
            Sleep(200 /*ms*/);
            continue;
            // gcov-end-ignore
        }
        else {
            break;
        }

        // Never reached.  The loop always either breaks or explicitly
        // continues.
    }

    if(fd == INVALID_HANDLE_VALUE) {
        // Map GetLastError() to errno, because we're not using mingw
        // or MSVCRT, which normally does it for us.
        // For now, only handle the most common cases
        DWORD err = GetLastError();
        if (efstreamDebug()) {
            cerr << "efstreambuf::open(\"" << f << "\", " << flags        // xnocover
                 << "): CreateFile failed with code " << err << endl;     // xnocover
        }
        if(err == ERROR_ACCESS_DENIED) {
            FileStats fstats;
            if(getFileStats(file, fstats)
               &&
               fstats.is_dir()) {
                throwDirError(f);
            }
        }
        errno = FilenameNS::getErrnoFromWindowsError(err);
        file_open_error::throw_last_OS_file_error
            (f,
             (flags & OPENFLAGS_WRITE) ?
             file_does_not_exist_error::OM_WRITE:
             file_does_not_exist_error::OM_READ);
    }
    buf_size = 0x1000;
#else
    int oflags = 0;
    if(flags & OPENFLAGS_READ) {
        if(flags & OPENFLAGS_WRITE) {
            oflags = O_RDWR;
        } else {
            oflags = O_RDONLY;
        }
    } else if(flags & OPENFLAGS_WRITE) {
        oflags = O_WRONLY;
    }
    if(flags & OPENFLAGS_TRUNCATE) {
        oflags |= O_TRUNC;
    }
    if(flags & OPENFLAGS_ALLOW_CREATE) {
        oflags |= O_CREAT;
        if(!(flags & OPENFLAGS_ALLOW_EXISTING)) {
            oflags |= O_EXCL;
        }
    }
    if(flags & OPENFLAGS_APPEND) {
        oflags |= O_APPEND;
    }
#ifdef HAVE_STAT64
    oflags |= O_LARGEFILE;
#endif
    // Set permissions: all = rv
    // The user's umask will remove the appropriate permissions
    // See bug 31543
    fd = ::open
        (f.toSystemDefaultString().c_str(), oflags,
         S_IWUSR | S_IRUSR |
         S_IWGRP | S_IRGRP |
         S_IWOTH | S_IROTH
        );
    if(fd < 0) {
        if(errno == EISDIR) {
            throwDirError(f);
        }
        file_open_error::throw_last_OS_file_error
            (f,
             (flags & OPENFLAGS_WRITE) ?
             file_does_not_exist_error::OM_WRITE:
             file_does_not_exist_error::OM_READ);
    }
    if(!(flags & OPENFLAGS_INHERIT)) {
        int rv = fcntl(fd, F_SETFD, FD_CLOEXEC);
        if(rv < 0) {
            int prev_error = errno;
            // Ignore error in "close" at this point
            ::close(fd);
            errno = prev_error;
            throw_XSystem("fcntl(FD_CLOEXEC)");
        }
    }

    // Retrieve the "optimal" buffer size from 'fstat'.
    FileStats fstats;
    buf.resize(2);           // 'getStats' wants this
    getStats(fstats);
    buf_size = fstats.io_buf_size;

    // It turns out that Solaris (and possibly FreeBSD, according to
    // the internet, maybe others) will allow read() of a
    // directory--if the directory is on certain file systems!  For
    // example, on b-solarisx86-01 (Solaris/x86 5.10), I can read "/"
    // and "/data00" but not "/tmp" or "/nfs".  Linux consistently
    // refuses to read() a directory.
    //
    // This causes problems because it means that if a user mistakenly
    // supplies a directory name to something that expects a file
    // name, then depending on the OS and the file system, we might
    // try to process it as a file, resulting in strange behavior.
    // Bug 34892 is related.
    //
    // Therefore, I'll take advantage of the 'fstat' we're already
    // doing to catch the case of attempting to read a directory here,
    // rather than relying on a subsequent 'read' call to fail.
    if (fstats.is_dir()) {
        try {
            close();
        }
        catch (...) {
            // ignore errors from 'close'
        }

        // Almost certainly this arises because of a bad file name
        // provided on a command line, so I'll classify it as a user
        // error here.
        throwDirError(f);
    }

    if (flags & OPENFLAGS_DELETE_ON_CLOSE) {
        if (0 != unlink(f.toSystemDefaultString().c_str())) {
            throw_XSystem("unlink");
        }
    }

#endif
    buf.resize(buf_size);
}

void efstreambuf::close() {
    if(!isOpen())
        return;
    flushWriteBuffer();
    buf.clear();
    file = Filename(SSE_SYSTEM_DEFAULT);
    setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
#ifdef __MC_MINGW__
    if(!CloseHandle(fd)) {
        throw_XSystem("CloseHandle");
    }
#else
    if(::close(fd)) {
        throw_XSystem("close");
    }
#endif
}

efstreambuf::~efstreambuf() {
    if(std::uncaught_exception()) {
        try {
            close();
        } catch(exception &e) {
            cerr << "efstreambuf::~efstreambuf: Exception during exception handling: "
                 << e.what() << endl;
        }
    } else {
        close();
    }
}

char *efstreambuf::buf_begin() {
    return &buf.front();
}

size_t efstreambuf::buf_size() const {
    return buf.size();
}

char *efstreambuf::buf_end() {
    return buf_begin() + buf_size();
}

bool efstreambuf::isOpen() const {
    return !buf.empty();
}

Filename efstreambuf::getFilename() const {
    return file;
}

efstreambuf::file_descriptor_t efstreambuf::getFileDescriptor() const {
    return fd;
}

bool efstreambuf::convertCRLF() const {
    return flags & OPENFLAGS_CONVERT_CRLF;
}

static size_t writeToFd
(efstreambuf::file_descriptor_t fd,
 const char *buf,
 size_t n) {
#ifdef __MC_MINGW__
        DWORD dwNumberOfBytesWritten = 0;
        if(!WriteFile(fd, buf, n,
                     &dwNumberOfBytesWritten,
                     /*lpOverlapped*/NULL)) {
            throw_XSystem("WriteFile");
        }
        return dwNumberOfBytesWritten;
#else
        ssize_t numberOfBytesWritten = 
            write(fd, buf, n);
        if(numberOfBytesWritten < 0) {
            throw_XSystem("write");
        }
        return numberOfBytesWritten;
#endif
}

static size_t readFromFd
(efstreambuf::file_descriptor_t fd,
 char *buf,
 size_t n) {
#ifdef __MC_MINGW__
        DWORD dwNumberOfBytesRead = 0;
        if(!ReadFile(fd, buf, n,
                     &dwNumberOfBytesRead,
                     /*lpOverlapped*/NULL)) {
            throw_XSystem("ReadFile");
        }
        return dwNumberOfBytesRead;
#else
        ssize_t numberOfBytesRead = 
            read(fd, buf, n);
        if(numberOfBytesRead < 0) {
            throw_XSystem("read");
        }
        return numberOfBytesRead;
#endif
}

int efstreambuf::sync() {
    flushWriteBuffer();
    return 0;
}

void efstreambuf::resizeBuffer(size_t newSize) {
    flushWriteBuffer();
    if(newSize < 2) {
        // We need at least 1 character for peek()
        // I think we might need a second for some CRLF related
        // thing.
        newSize = 2;
    }
    buf.resize(newSize);
    setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
}

void efstreambuf::flushWriteBuffer() {
    string_assert(isOpen(), "Trying to operate on unopen file");
    const char *cur = pbase();
    const char *end = pptr();

#ifdef __MC_MINGW__
    // In "append" mode on Windows, we need to seek to the end every
    // time we write.
    // Removing the "FILE_WRITE_DATA" permission doesn't appear to
    // work with ClearCase; and only seeking on opening doesn't work
    // with interleaved writes, such as between cov-build and
    // cov-internal-capture
    // It seems like there could be race conditions with this, but
    // this appears to be what the MSVCRT does.
    if(cur != end && (flags & OPENFLAGS_APPEND)) {
        // Seek to the end.
        if(SetFilePointer
           (fd,
            /*distanceToMove*/ 0,
            /*distanceToMoveHigh*/ NULL,
            FILE_END
           ) == INVALID_SET_FILE_POINTER) {
            throw_XSystem("SetFilePointer");
        }
    }
#endif

    while(cur != end) {
        const char *lf;
        if(convertCRLF()
           &&
           (lf = (const char *)memchr(cur, '\n', end - cur)) != NULL) {
            // Turn LF into CRLF
            static const char crlf[2] = {
                '\r','\n'
            };
            size_t to_write = lf - cur;
            size_t written = writeToFd(fd, cur, to_write);
            if(written == to_write) {
                // Assume we can always write 2 bytes.
                size_t two_bytes = writeToFd(fd, crlf, 2);
                cond_assert(two_bytes == 2);
                // Only add "1" to written, because we only wrote the
                // '\n' from the buffer
                ++written;
            }
            cur += written;
        } else {
            cur += writeToFd(fd, cur, end - cur);
        }
    }
    setp(NULL, NULL);
}

std::streampos efstreambuf::seekoff
(std::streamoff off,
 std::ios_base::seekdir way,
 std::ios_base::openmode which) {
    // Write current buffer, if any, before seeking
    flushWriteBuffer();
    long long to_end_of_buffer = 0;
    if(way == std::ios_base::cur) {
        // For "cur", we need to take into account the current read
        // buffer: the current position is not the underlying file
        // pointer.
        // We need to reset "off" so that it's relative to the file
        // pointer, i.e. the end of the get buffer.
        to_end_of_buffer =
            egptr() - gptr();
        if(off < to_end_of_buffer
           &&
           off >= eback() - gptr()) {
            // If we can stay within the read buffer, do it.
            gbump(off);
            // Offset from the end of the get buffer to new current position
            to_end_of_buffer = egptr() - gptr();
            // Set "off" to 0: we don't want to actually move the file
            // pointer, just get the current file position.
            off = 0;
        } else {
            // Adjust offset to be based on end of buffer (i.e. file
            // pointer)
            off -= to_end_of_buffer;
            // Disable adjustment of return value.
            to_end_of_buffer = 0;
            // Clear buffer
            setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
            trailing_cr = FALSE;
        }
    } else {
        // Not a relative seek; discard buffer because we don't know
        // (not easily anyway) which part of the file it actually
        // represents.
        setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
        trailing_cr = FALSE;
    }
    unsigned long long rv;
#ifdef __MC_MINGW__
    DWORD dwMoveMethod;
    switch(way) {
    case std::ios_base::beg:
        dwMoveMethod = FILE_BEGIN;
        break;
    case std::ios_base::cur:
        dwMoveMethod = FILE_CURRENT;
        break;
    case std::ios_base::end:
        dwMoveMethod = FILE_END;
        break;
    default:
        xfailure("Invalid seekdir");
    }
    LONG lo = off;
    LONG hi = off >> 32;
    if((lo = SetFilePointer
       (fd,
        lo,
        &hi,
        dwMoveMethod
        )) == INVALID_SET_FILE_POINTER) {
        throw_XSystem("SetFilePointer");
    }
    rv = (long long)lo | ((long long)hi << 32);
#else
    int whence;
    switch(way) {
    case std::ios_base::beg:
        whence = SEEK_SET;
        break;
    case std::ios_base::cur:
        whence = SEEK_CUR;
        break;
    case std::ios_base::end:
        whence = SEEK_END;
        break;
    default:
        xfailure("Invalid seekdir");
    }
#ifdef HAVE_STAT64
#define lseek lseek64
#define off_t off64_t
#endif
    off_t newOff = lseek(fd, off, whence);
    if(newOff == -1) {
        throw_XSystem("lseek");
    }
    rv = newOff;
#endif
    if(way == std::ios_base::cur) {
        rv -= to_end_of_buffer;
    }
    return rv;
}

std::streampos efstreambuf::seekpos
(std::streampos sp,
 std::ios_base::openmode which) {
    return seekoff(sp, std::ios_base::beg, which);
}

efstreambuf::traits_type::int_type efstreambuf::overflow(traits_type::int_type ch) {
    flushWriteBuffer();
    // Clear the get buffer.
    setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
    setp(buf_begin(), buf_end());
    if(traits_type::eq_int_type(ch, traits_type::eof())) {
        return ch;
    }
    *pptr() = ch;
    pbump(1);
    return ch;
}

std::streamsize efstreambuf::xsputn
(const traits_type::char_type* s, streamsize n) {
    if(!convertCRLF()
       &&
       n > buf_size() * 2) {
        flushWriteBuffer();
        // Clear the get buffer.
        setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
        return writeToFd(fd, s, n);
    } else {
        return streambuf::xsputn(s, n);
    }
}

// Input
streamsize efstreambuf::showmanyc() {
    // TODO: Make this better.
    // For now, I don't think we use it anyway.
    return streambuf::showmanyc();
}

streamsize efstreambuf::xsgetn (traits_type::char_type* s, streamsize n) {
    flushWriteBuffer();
    size_t available = egptr() - gptr();
    // If we would fill more than one bufferful, read directly to
    // the buffer instead.
    if(!convertCRLF() && n >= buf_size() + available) {
        memcpy(s, gptr(), available);
        // Indicate we've emptied our read buffer
        setg(/*gbeg*/ NULL, /*gnext*/ NULL, /*gend*/ NULL);
        streamsize rv = available;
        n -= available;
        s += available;
        return rv + readFromFd(fd, s, n);
    } else {
        return streambuf::xsgetn(s, n);
    }
}

// "memstr("\r\n")" but memstr doesn't exist
// TODO: Implement memstr.
// I'm not sure what the most efficient way to do it is; create
// automaton? Only do it for "sufficiently large" "needle" ?
static char *find_CRLF(char *buf, size_t n) {
    char *cr;
    while(n >= 2
          &&
          (cr = (char *)memchr(buf, '\r', n - 1)) != NULL) {
        if(cr[1] == '\n')
            return cr;
        // Skip the CR, look for next one
        ++cr;
        n -= cr - buf;
        buf = cr;
    }
    return NULL;
}

efstreambuf::traits_type::int_type efstreambuf::underflow() {
    flushWriteBuffer();
    size_t to_read = buf_size();
    char *loc_buf = buf_begin();

    // Look for a trailing '\r' in our current buffer (see below for
    // it being stored)
    // If present, move it to the beginning of the buffer.
    if(trailing_cr) {
        trailing_cr = false;
        *loc_buf++ = '\r';
        --to_read;
    }
           
    size_t n =
        readFromFd(fd, loc_buf, to_read);
    // n == 0 means EOF.
    // However, we may have an extra CR to include in input.
    if(!n && loc_buf == buf_begin()) {
        // At EOF.
        return traits_type::eof();
    }
    char *end = loc_buf + n;
    if(convertCRLF()) {
        // turn CRLF into LF
        // Include the leading '\r' if applicable
        char *wbuf = buf_begin();
        char *rbuf = buf_begin();

        while(char *cr = find_CRLF(rbuf, end - rbuf)) {
            size_t to_copy = cr - rbuf;
            if(wbuf != rbuf)
                memmove(wbuf, rbuf, to_copy);
            wbuf += to_copy;
            *wbuf++ = '\n';
            rbuf = cr + 2;
        }
        if(rbuf < end) {
            size_t to_copy = end - rbuf;
            if(wbuf != rbuf) {
                memmove(wbuf, rbuf, to_copy);
            }
            wbuf += to_copy;
        }
        end = wbuf;
        // If there is a trailing '\r', keep it in "buf", but not in
        // the streambuf's "get buffer" (because if might be
        // suppressed by the nexdt underflow)
        // If there's no trailing '\r', mark that by storing a 0 if
        // there's space for it.
        // Do not remove it if we had an EOF, because then we know
        // it's alone.
        if(n) {
            if(*(end - 1) == '\r') {
                --end;
                trailing_cr = true;
                if(end == buf_begin()) {
                    // We can't return with an empty buffer.
                    // Recurse to get more data, or reach EOF and
                    // leave the \r in the get buffer.
                    return underflow();
                }
            }
        }
    }
    setg(buf_begin(), buf_begin(), end);
    return traits_type::to_int_type(*gptr());
}

const OpenFlags eifstream::readFlags =
    OPENFLAGS_READ;

eifstream::eifstream():
    istream(&buf) {
    exceptions(ios::badbit);
}

eifstream::eifstream
(const Filename &f,
 TextModeFlag mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const Filename &f,
 OpenFlags mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const path &f,
 TextModeFlag mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const path &f,
 OpenFlags mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const char *f,
 TextModeFlag mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const char *f,
 OpenFlags mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const string &f,
 TextModeFlag mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eifstream::eifstream
(const string &f,
 OpenFlags mode):
    istream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

void eifstream::open
(const Filename &f,
 TextModeFlag mode) {
    OpenFlags flags = readFlags;
    if(mode == text) {
        flags |= OPENFLAGS_TEXT;
    } else {
        flags |= OPENFLAGS_BINARY;
    }
    open(f, flags);
}

void eifstream::open
(const Filename &f,
 OpenFlags flags) {
    clear();
    buf.open(f, flags | readFlags);
}

void eifstream::open
(const char *f,
 TextModeFlag mode) {
    open(Filename(f, Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void eifstream::open
(const char *f,
 OpenFlags mode) {
    open(Filename(f, Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void eifstream::open
(const string &f,
 TextModeFlag mode) {
    open(Filename(f.c_str(), Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void eifstream::open
(const string &f,
 OpenFlags mode) {
    open(Filename(f.c_str(), Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void eifstream::open
(const path &f,
 TextModeFlag mode) {
    open(Filename(f, Filename::FROM_BOOST_PATH, SSE_SYSTEM_DEFAULT), mode);
}

void eifstream::open
(const path &f,
 OpenFlags mode) {
    open(Filename(f, Filename::FROM_BOOST_PATH, SSE_SYSTEM_DEFAULT), mode);
}

void eifstream::close() {
    buf.close();
}

bool eifstream::is_open() const {
    return buf.isOpen();
}

const OpenFlags eofstream::truncateFlags =
    OPENFLAGS_WRITE |
    OPENFLAGS_ALLOW_EXISTING |
    OPENFLAGS_ALLOW_CREATE |
    OPENFLAGS_TRUNCATE;

const OpenFlags eofstream::appendFlags =
    OPENFLAGS_WRITE |
    OPENFLAGS_ALLOW_EXISTING |
    OPENFLAGS_ALLOW_CREATE |
    OPENFLAGS_APPEND;

eofstream::eofstream():
    ostream(&buf) {
    exceptions(ios::badbit);
}

eofstream::eofstream
(const Filename &f,
 TextModeFlag mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream
(const Filename &f,
 OpenFlags mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream(const path &f,
 TextModeFlag mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream(const path &f,
 OpenFlags mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream(const char *f,
 TextModeFlag mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream(const char *f,
 OpenFlags mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream(const string &f,
 TextModeFlag mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

eofstream::eofstream(const string &f,
 OpenFlags mode):
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

void eofstream::open(const Filename &f,
 TextModeFlag mode) {
    clear();
    OpenFlags flags =
        truncateFlags;
    if(mode == text) {
        flags |= OPENFLAGS_TEXT;
    } else {
        flags |= OPENFLAGS_BINARY;
    }
    open(f, flags);
}

void eofstream::open(const Filename &f,
                     OpenFlags flags) {
    clear();
    buf.open(f, flags | OPENFLAGS_WRITE);
}

void eofstream::open(const char *f,
                     TextModeFlag mode) {
    open(Filename(f, Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void eofstream::open(const char *f,
                     OpenFlags mode) {
    open(Filename(f, Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void eofstream::open(const path &f,
                     TextModeFlag mode) {
    open(f.native_file_string().c_str(), mode);
}

void eofstream::open(const path &f,
                     OpenFlags mode) {
    open(f.native_file_string().c_str(), mode);
}

void eofstream::open(const string &f,
                     TextModeFlag mode) {
    open(f.c_str(), mode);
}

void eofstream::open(const string &f,
                     OpenFlags mode) {
    open(f.c_str(), mode);
}

void eofstream::close() {
    buf.close();
}

bool eofstream::is_open() const {
    return buf.isOpen();
}

const OpenFlags efstream::truncateFlags =
    eofstream::truncateFlags | eifstream::readFlags;

const OpenFlags efstream::appendFlags =
    eofstream::appendFlags | eifstream::readFlags;

const OpenFlags efstream::temporaryFile =
    OPENFLAGS_WRITE |
    OPENFLAGS_ALLOW_CREATE |
    OPENFLAGS_TRUNCATE |
    OPENFLAGS_DELETE_ON_CLOSE |
    eifstream::readFlags;

efstream::efstream():
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
}

efstream::efstream
(const Filename &f,
 TextModeFlag mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream
(const Filename &f,
 OpenFlags mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream(const path &f,
 TextModeFlag mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream(const path &f,
 OpenFlags mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream(const char *f,
 TextModeFlag mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream(const char *f,
 OpenFlags mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream(const string &f,
 TextModeFlag mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

efstream::efstream(const string &f,
 OpenFlags mode):
    istream(&buf),
    ostream(&buf) {
    exceptions(ios::badbit);
    open(f, mode);
}

void efstream::open(const Filename &f,
 TextModeFlag mode) {
    clear();
    OpenFlags flags =
        truncateFlags;
    if(mode == text) {
        flags |= OPENFLAGS_TEXT;
    } else {
        flags |= OPENFLAGS_BINARY;
    }
    open(f, flags);
}

void efstream::open(const Filename &f,
                     OpenFlags flags) {
    clear();
    buf.open(f, flags | eifstream::readFlags | OPENFLAGS_WRITE);
}

void efstream::open(const char *f,
                     TextModeFlag mode) {
    open(Filename(f, Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void efstream::open(const char *f,
                     OpenFlags mode) {
    open(Filename(f, Filename::FI_HOST, SSE_SYSTEM_DEFAULT), mode);
}

void efstream::open(const path &f,
                     TextModeFlag mode) {
    open(f.native_file_string().c_str(), mode);
}

void efstream::open(const path &f,
                     OpenFlags mode) {
    open(f.native_file_string().c_str(), mode);
}

void efstream::open(const string &f,
                     TextModeFlag mode) {
    open(f.c_str(), mode);
}

void efstream::open(const string &f,
                     OpenFlags mode) {
    open(f.c_str(), mode);
}

void efstream::close() {
    buf.close();
}

bool efstream::is_open() const {
    return buf.isOpen();
}

intfd_ofstream::intfd_ofstream(): ofstream() {
    exceptions(std::ios_base::badbit);
        
}

void intfd_ofstream::open(const Filename &f,
                         std::ios_base::openmode openmode) {
    openmode |= std::ios_base::out;
    ofstream::open(f.toString().c_str(),
                   openmode);
    if(fail()) {
        file_open_error::throw_last_OS_file_error
            (f, file_open_error::OM_WRITE);
    }
}


// ----------------------- Test code --------------------------
static void checkCurrentChar(istream &in, unsigned char c) {
    int n = in.peek();
    string_assert(n != EOF, "Expected a char, got EOF");
    ostr_assert(n == (int)c, "Got '"
                << (char)n << "' but expected '"
                << (char)c << "'");
}

static void checkSeek(istream &in,
               std::streampos pos,
               ios_base::seekdir way,
               std::streampos expectedNewpos
               ) {
    in.seekg(pos, way);
    std::streampos newpos = in.tellg();
    ostr_assert(newpos == expectedNewpos,
                "Tried to seek to "
                << expectedNewpos
                << " but we're now at "
                << newpos);
    if(way == ios_base::beg) {
        in.seekg(0);
        string_assert(in.tellg() == std::streampos(0), "Failed to seek to start");
        in.seekg(pos);
        std::streampos newpos = in.tellg();
        ostr_assert(newpos == expectedNewpos,
                    "Tried to seek to "
                    << expectedNewpos
                    << " but we're now at "
                    << newpos);
    }
}

static void checkSeek(
    ostream &out,
    std::streampos pos,
    ios_base::seekdir way,
    std::streampos expectedNewpos
) {
    out.seekp(pos, way);
    std::streampos newpos = out.tellp();
    ostr_assert(newpos == expectedNewpos,
                "Tried to seek to "
                << expectedNewpos
                << " but we're now at "
                << newpos);
    if(way == ios_base::beg) {
        // In this case, the no-way-argument version is equivalent; it
        // uses a different call chain so test it separately.
        out.seekp(0);
        ostr_assert(out.tellp() == std::streampos(0), "Failed to seek to start");
        out.seekp(pos);
        std::streampos newpos = out.tellp();
        ostr_assert(newpos == expectedNewpos,
                    "Tried to seek to "
                    << expectedNewpos
                    << " but we're now at "
                    << newpos);
    }
}

static void checkFileContents(const Filename &file,
                              const string &contents) {
    eifstream in(file, eifstream::binary);
    ostringstream ostr;
    copy_stream(ostr, in);
    ostr_assert(ostr.str() == contents,
                "Expected \"" << escapeCString(contents)
                << "\" but got \""
                << escapeCString(ostr.str()) << '"');
}

static void test_data(const Filename &f,
               const string &test_bin,
               const string &test_crlf_read,
               const string &test_crlf_written) {
    eifstream in;
    eofstream out;
    out.open(f, eofstream::binary);
    out << test_bin;
    out.close();
    ostringstream ostr;
    checkFileContents(f, test_bin);
    in.open(f, OPENFLAGS_CONVERT_CRLF | OPENFLAGS_BINARY);
    // Insert a buffer break between a '\r' and a '\n'
    in.buf.resizeBuffer(9);
    ostr.str("");
    copy_stream(ostr, in);
    ostr_assert(ostr.str() == test_crlf_read,
                "Expected \"" << escapeCString(test_crlf_read)
                << "\" but got \""
                << escapeCString(ostr.str()) << '"');
    out.open(f, eofstream::binary);
    out.buf.resizeBuffer(test_bin.size() * 2 / 3);
    // Make sure we fill some bufferfuls
    static const int count = 0x1000;
    for(int i = 0; i < count; ++i) {
        out << test_bin;
    }
    out.close();
    in.open(f, eifstream::binary);
    in.buf.resizeBuffer(test_bin.size() * 2 / 3);
    FileStats stats;
    in.buf.getStats(stats);
    cond_assert(stats.size == count * test_bin.size());
    vector<char> buf(test_bin.size());
    for(int i = 0; i < count; ++i) {
        in.read(&buf.front(), buf.size());
        cond_assert(in.gcount() == buf.size());
        ostr_assert(!memcmp(&buf.front(), test_bin.data(), buf.size()),
                    "After " << i << " iterations, "
                    "expected \"" << escapeCString(test_bin)
                    << "\" but got \""
                    << escapeCString(&buf.front(), buf.size()) << '"'
                    );
    }
    in.close();
    out.open(f, OPENFLAGS_CONVERT_CRLF |
             OPENFLAGS_TRUNCATE |
             OPENFLAGS_ALLOW_EXISTING | OPENFLAGS_BINARY);
    out.buf.resizeBuffer(9);
    out << test_bin;
    out.close();
    ostr.str("");
    in.open(f, eifstream::binary);
    copy_stream(ostr, in);
    in.close();
    ostr_assert(ostr.str() == test_crlf_written,
                "Expected \"" << escapeCString(test_crlf_written)
                << "\" but got \""
                << escapeCString(ostr.str()) << '"');
}

static void testEfstream(Filename &f) {
    // Include:
    // - an \r\n separated by a buffer break
    // - a buffer break without \r\n
    // - a buffer break preceded by \r<not \n>
    // - a trailing \r
    // - a \r not followed by \n
    // Not that a \r at the end of a buffer will still be in the next buffer
    string test_bin
        ("abc\r\ncde\r" // 9 chars
         "\naaaa\rb" // 8 chars, in lcude previous \r
         "bbbbbbbb\r" // 9 chars
         "afg\r\n\r\n\r");
    // Version with CRLF turned into LF
    string test_crlf_read
        ("abc\ncde"
         "\naaaa\rb"
         "bbbbbbbb\r"
         "afg\n\n\r");
    // Version with LF turned into CRLF
    string test_crlf_written
        ("abc\r\r\ncde\r" // 9 chars
         "\r\naaaa\rb" // 8 chars, in lcude previous \r
         "bbbbbbbb\r" // 9 chars
         "afg\r\r\n\r\r\n\r");
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    test_bin = "";
    test_crlf_read = "";
    test_crlf_written = "";
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    test_bin = "\n";
    test_crlf_read = "\n";
    test_crlf_written = "\r\n";
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    test_bin = "\r";
    test_crlf_read = "\r";
    test_crlf_written = "\r";
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    test_bin = "\r\n";
    test_crlf_read = "\n";
    test_crlf_written = "\r\r\n";
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    test_bin = "\r\r";
    test_crlf_read = "\r\r";
    test_crlf_written = "\r\r";
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    // No CR or LF, more than 1 buffer
    test_bin = "aaaaaaaaaa";
    test_crlf_read = "aaaaaaaaaa";
    test_crlf_written = "aaaaaaaaaa";
    test_data(f, test_bin, test_crlf_read,
              test_crlf_written);

    eofstream out;
    eifstream in;

    // Test appending / truncating
    FileStats stats;
    out.open(f, OPENFLAGS_WRITE | OPENFLAGS_TRUNCATE | OPENFLAGS_BINARY);
    out.buf.getStats(stats);
    ostr_assert(stats.size == 0, "Expected file to have been truncated, "
                "but its size is " << stats.size);
    out << "1234";
    out.buf.getStats(stats);
    ostr_assert(stats.size == 4, "Expected size to be 4 now, "
                "but it's " << stats.size);
    out.close();
    out.open(f, OPENFLAGS_WRITE | OPENFLAGS_APPEND | OPENFLAGS_BINARY);
    out.buf.getStats(stats);
    ostr_assert(stats.size == 4, "Expected size to still be 4, "
                "but it's " << stats.size);
    out << "56789";
    out.buf.getStats(stats);
    ostr_assert(stats.size == 9, "Expected size to be 9 now, "
                "but it's " << stats.size);
    out.close();

    // Read seeking
    in.open(f, eifstream::text);
    in.buf.resizeBuffer(5);
    checkCurrentChar(in, '1');
    // Move around within the buffer
    checkSeek(in, 3, ios_base::cur, 3);
    checkCurrentChar(in, '4');
    checkSeek(in, -2, ios_base::cur, 1);
    checkCurrentChar(in, '2');
    // Got past buffer end
    checkSeek(in, 5, ios_base::cur, 6);
    checkCurrentChar(in, '7');
    checkSeek(in, 5, ios_base::beg, 5);
    checkCurrentChar(in, '6');
    checkSeek(in, -1, ios_base::end, 8);
    checkCurrentChar(in, '9');
    checkSeek(in, 0, ios_base::beg, 0);
    checkCurrentChar(in, '1');
    in.close();

    // Write seeking
    // File contains "123456789" right now.
    out.open(f, OPENFLAGS_BINARY);
    out.buf.resizeBuffer(5);
    checkSeek(out, 3, ios_base::cur, 3);
    out.write("987654", 6);
    out.close();
    const char *lastFileContents = "123987654";
    checkFileContents(f, lastFileContents);

    {
        // Test xsgetn, which had a bug when reading more than the buffer
        // size.
        // This relies on the contents set above.
        in.open(f, OPENFLAGS_READ | OPENFLAGS_BINARY);
        in.buf.resizeBuffer(5);
        // Fill buffer.
        char buf[strlen(lastFileContents) + 10];
        static const int prefixSize = 2;
        in.read(buf, prefixSize);
        cond_assert(in.gcount() == prefixSize);
        cond_assert(!memcmp(buf, lastFileContents, in.gcount()));
        in.read(buf + prefixSize, sizeof(buf) - prefixSize);
        cond_assert(in.gcount() == strlen(lastFileContents) - prefixSize);
        cond_assert(!memcmp(buf, lastFileContents, strlen(lastFileContents)));
    }

    
    // Try some expected failures
    try {
        cout << "Trying to create a file that already exists" << endl;
        // ALLOW_CREATE denies ALLOW_EXISTING if not explictly specified
        out.open(f, OPENFLAGS_ALLOW_CREATE | OPENFLAGS_BINARY);
        xfailure("Shouldn't be allowed to open existing");
    } catch(file_open_error &exn) {
        cout << "Expected error: " << exn.what_short() << endl;
        cond_assert(exn.errnoCode == EEXIST);
    }
    out.close();
    remove(f.toString());
    try {
        cout << "Trying to open a file that doesn't exist" << endl;
        in.open(f, OPENFLAGS_READ | OPENFLAGS_BINARY);
        xfailure("Shouldn't be allowed to open non-existing");
    } catch(file_open_error &exn) {
        cout << "Expected error: " << exn.what_short() << endl;
        cond_assert(exn.errnoCode == ENOENT);
    }
    in.close();

    // Make sure we can't read a directory.
    try {
        eifstream dir(".", OPENFLAGS_BINARY);
        xfailure("We should not have been able to open a directory");
    }
    catch (XUserError &x) {
        cout << "Attempting to read a directory yielded expected error: "
             << x.why << endl;
    }
}

static void test_efstream(const char *directory)
{
    Filename dir(directory, Filename::FI_PORTABLE);
    cond_assert(dir_exists(dir));
    Filename fileName = dir / "temp_file";
    string input("Hello World!\x01\x02\x03");
    cond_assert(!file_exists(fileName));
    {
        efstream file;
        file.open(fileName, efstream::temporaryFile | OPENFLAGS_BINARY);
        file.write(input.c_str(), input.size());
        file.seekg(0);
        char value[80];
        file.read(value, input.size());
        cond_assert(file);
        string result(value, input.size());
        cond_assert(result == input);
    }
    cond_assert(!file_exists(fileName));
}

static void testTempFile(const char *directory)
{
    Filename dir(directory, Filename::FI_PORTABLE);
    cond_assert(dir_exists(dir));
    Filename fileName = dir / "temp_file";
    string input("HelloWorld!");
    cond_assert(!file_exists(fileName));
    {
        efstream file;
        file.open(fileName, efstream::temporaryFile | OPENFLAGS_TEXT);
        file << input;
        file.seekg(0);
        string value;
        file >> value;
        cond_assert(value == input);
    }
    cond_assert(!file_exists(fileName));

    // Create this file just to verify that the makefile driving this test
    // is correct.
    Filename persistFileName = dir / "persist_file";
    eofstream persistFile;
    persistFile.open(persistFileName, eofstream::truncateFlags | OPENFLAGS_TEXT);
    persistFile << input;
    persistFile.close();

    // Deliberately leak this file so that it's destructor is never called.
    // Its existance is tested in the Makefile after the unit test program
    // exits.
    efstream *tempFile = new efstream();
    cond_assert(!file_exists(fileName));
    tempFile->open(fileName, efstream::temporaryFile | OPENFLAGS_TEXT);
    *tempFile << input;
    tempFile->flush();
}


static void testEfstreamConcurrency(int childNum)
{
    cout << "testEfstreamConcurrency start, childNum=" << childNum << endl;

    Filename f("tmp/concur-file.txt", Filename::FI_PORTABLE);

    // Repeatedly write a file while other processes are doing the
    // same.  The main purpose is to demonstrate that we will not
    // fail, contrary to the behavior in bug 62701.
    for (int i=0; i < 10000; i++) {
        eofstream ostr(f,
                       OPENFLAGS_WRITE | OPENFLAGS_ALLOW_CREATE
                       | OPENFLAGS_ALLOW_EXISTING | OPENFLAGS_TRUNCATE
                       | OPENFLAGS_TEXT);
        ostr << "child" << childNum << endl;
    }

    cout << "testEfstreamConcurrency finish, childNum=" << childNum << endl;
}


void efstream_unit_tests(int argc, const char * const *argv) {
    Filename f(SSE_UTF8);
    if(argc < 2) {
        const char * const directory = "tmp";
        create_directories(directory);
        f = Filename("tmp/efstream.tmp", Filename::FI_PORTABLE, SSE_SYSTEM_DEFAULT);
        testEfstream(f);
        f = Filename("tmp/efstream.tmp", Filename::FI_PORTABLE, SSE_UTF8);
        testEfstream(f);
        test_efstream(directory);
        testTempFile(directory);
    }

    else if (str_equal(argv[1], "--child")) {
        // This is run from libs/unit-tests/test-efstream-concurrency.sh.
        testEfstreamConcurrency((int)string_to_int(argv[2], /*throwOnError=*/ true));
        return;
    }

    else {
        // smcpeak: As far as I can tell, this code is only reachable
        // when someone manually invokes the unit-tests binary with
        // a file name.
        f = Filename(argv[1], Filename::FI_HOST, SSE_SYSTEM_DEFAULT);
        testEfstream(f);
    }

    // Call this function just so that gcc does not warn about it
    // not being used when we are compiling for non-Windows.
    (void)efstreamDebug();
}

// EOF
