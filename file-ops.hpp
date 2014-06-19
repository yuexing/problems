// filename:
//   filesystem root: optional name of a filesystem, e.g. a
//     Windows drive letter
//   absolute marker: optional marker indicating name is absolute
//   sequence of names: possibly empty sequence of names, from
//     the name of the uppermost directory down to the final
//     component of the file name in question
//
// getcwd , chdir(SetCurrentDirectory)

// popular file operations

//
struct FileStats {
    enum FileKind {
        // Directory
        FK_DIRECTORY,
        // Regular file, not a link
        FK_REGULAR,
        // Symbolic link
        FK_LINK,
        // Other, e.g. character device
        FK_OTHER
    } kind;
    bool is_dir() const {
        return kind == FK_DIRECTORY;
    }
    bool is_reg() const {
        return kind == FK_REGULAR;
    }
    bool is_link() const {
        return kind == FK_LINK;
    }

    long last_access;
    long last_modification;
    unsigned long long size;
    // "Optimal" buffer size for I/O, as returned by "stat".
    // 0 if it can't be determined (e.g. on Windows)
    int io_buf_size;
};

bool handle_error(int ret)
{
    if(!ret) {
        return true;
    } else {
        sprintf("%s\n", strerror(ret));
    }
}

void fill_stat(FileStats &buf, const struct stat &s)
{
    switch(s.st_mode) {
        case S_IFDIR:
            buf.kind = FK_DIRECTORY;
            break;
        case S_IFREG:
            buf.kind = FK_REGULAR;
            break;
        case S_IFLNK:
            buf.kind = FK_LINK;
            break;
        default:
            buf.kind = FK_OTHER;
            break;
    }

    buf.last_access = s.st_atime.sec;
    buf.last_modification = s.st_mtime.sec;
    buf.size = s.st_size;
    buf.io_buf_size = s.st_blksize;
}

bool getFileStats(const char *f, FileStats &buf)
{
    struct stat s;
    if(handle_error(stat(f, &s))) {
        fill_state(buf, s);
        return true;
    }
    return false;
}

bool getLinkStats(const char *f, FileStats &buf)
{
    struct stat s;
    if(handle_error(lstat(f, &s))) {
        fill_state(buf, s);
        return true;
    }
    return false;
}

// chmod
void set_file_writable(const char *f)
{}

void set_file_ro(const char* f)
{}

void set_last_write_time(const char* f, long time)
{
    
}
void set_last_write_time_now(const char *f)
{
    set_last_write_time(f, timestamp());
}


void do_touch(const char* f)
{
    close(open(f, O_RDONLY | O_CREAT));
}

// file_exists
void dir_exists(const char* f)
{
    FileStats buf;
    return getFileStats(f, buf) && buf.is_dir();
}

// compare the st_dev/st_ino is enough
// or just string compare?
bool same_file(const char* f1, const char* f2)
{
   return !str_cmp(f1, f2);
}

// how?
void change_dir(const char *f)
{
    
}

// dup2
bool redirect_stdout(const char* f)
{
    fflush(stdout);
    int fd = open(f, O_RDWR|O_CREAT);
    if(handel_eror(fd)) {
        dup2(fd, STDOUT_FILENO);
        return true;
    }
    return false;
}

bool redirect_stderr(const char* f)
{
    fflush(stderr);
    int fd = open(f, O_RDWR|O_CREAT);
    if(handel_eror(fd)) {
        dup2(fd, STDERR_FILENO);
        return true;
    }
    return false;
}

bool redirect_all(const char* f)
{
    return redirect_stdout(f) || redirect_stderr(f);
}

// check string is enough
bool is_subdirectory(const char* p1, const char* p2)
{
    if(const char* idx = strstr(p1, p2))
    {
        idx += strlen(p1);
        return idx == 0 || *idx = '/'; 
    }
}

void force_remove_dir(const char *f)
{
    DIR *d = opendir(f);
    while(struct dirent *e = readdir(f)){
        // e->d_name
    }
}

void force_remove(const char* f)
{
    if(is_file(f)) {
        set_file_writable(f);
        unlink(f);
    } else {
        force_remove_dir(f);
    }
}

void copy_file(const char* f1, const char* f2)
{
    // read by the st_blksize    
}

void copy_files(const char *f1, const char *f2)
{
    // traverse, append a dir (recursively) or append a file
}

void create_directories(const char *f)
{
    for(int i = 0; f[i]; ++i) {
        if(f[i] == '/') {
            f[i] = 0;
            mkdir(f);
            f[i] = '/';
        }
    }
}

// make it absolute path
// inspect the string is enough
const char* make_relative(const char* f1, const char* f2)
{}

class DirEntries {
public:
    explicit DirEntries(const Filename &);

    class iterator : public std::iterator<std::input_iterator_tag, const Filename> {
    public:
        // Iterator on the files in "dir"
        iterator(const Filename &dir);
        // End iterator
        iterator();
        // We need to out-of-line these functions (even though the
        // default implementation works) so that "impl" is complete at
        // their definition site
        iterator(const iterator&);
        iterator &operator=(const iterator&);
        ~iterator();



        iterator &operator++();
        const Filename &operator*() const;
        const Filename *operator->() const {
            return &operator*();
        }

        // these use pointer equality, and comparing against anything other
        // than DirEntries::end/iterator() may yield unexpected results.
        bool operator==(const iterator &) const;
        bool operator!=(const iterator &i) const {
            return !operator==(i);
        }

    private:
        // This function starts the iteration if not started, checks
        // whether there are any actual files to iterate on, and if
        // not resets "impl_".
        void checkAtEnd() const;
        // We use a reference count with copy-on-write
        // It's common to copy a begin iterator around but only keep 1
        // copy at the time we modify it.
        class impl;
        // This contains the system data structures associated with
        // the iterator.
        // To prevent useless copying of system data structures, we
        // only compute them on a need basis (i.e. deref, increment or
        // comparison)
        // If unset, this is an end iterator.
        // Note that this can be set and still represent an "end"
        // iterator: if we copy a "begin" iterator to
        // an empty directory that's not "started", and then start the
        // copy, we can't reset this on the original one.
        // Still, if we can, we reset impl_ when we detect the end
        // condition.
        mutable refct_ptr<impl> impl_;
    };

    iterator begin() const;
    iterator end() const;

private:
    Filename dirname_;
};

class RecursiveDirEntries {
public:
    explicit RecursiveDirEntries(const Filename &dir):
        dirname_(dir) {
    }

    struct GetChildren;

    /**
     * An iterator on all the files/subdirectories of a given directory (recursively)
     * Directories are visited before their contents
     **/
    typedef recursive_iterator<DirEntries::iterator, GetChildren> iterator;

    struct GetChildren {
        pair<DirEntries::iterator, DirEntries::iterator> operator()(const Filename &dir);
    };

    iterator begin() const {
        return iterator(dirname_);
    }
    iterator end() const {
        return iterator();
    }
private:
    Filename dirname_;
};

/**
 * Sanitize paths to deal with some issues found on windows that cause 
 * problems with boost constructors, namely:
 * - trailing space
 * - trailing '.'
 * - doubled '\'
 * NB: the good part of string is that you do not need to worry about 
 * malloc/free
 **/
string sanitize_path(const string &filename);
