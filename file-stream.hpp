// Enhanced file-stream
// OPENFLAGS_CONVERT_CRLF, OPENFLAGS_DELETE_ON_CLOSE

class efstreambuf:
    public streambuf {
public:
    typedef int file_descriptor_t;

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
    void open(const char *f, OpenFlags flags);
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


