/* filename.c
 *
 * Handles regularizing all file names.  All backslashes are converted to
 * forward slashes.  The file name returned is either the file name minus
 * the root directory, or it still contains the root directory, depending
 * on whether or not the strip_root parameter is set in the get_file_name
 * call.
 *
 * (c) 2002 The Board of Trustees of the Leland Stanford Junior University
 * (c) 2004-2014 Coverity, Inc. All rights reserved worldwide.
 */

#ifdef __MC_MINGW__
#ifndef WINVER
#define WINVER 0x0501
#endif
#include <windows.h>
#include <io.h>
#include <share.h>
#endif

// Copied from covlk
#if defined (__MC_LINUX32__) || defined (__MC_HPUX__) || defined (__MC_SOLARIS__)
# define HAVE_STAT64 1
#endif

#ifdef HAVE_STAT64
# define _LARGEFILE64_SOURCE 1
# define _LARGEFILE_SOURCE 1
#endif /* HAVE_STAT64 */
// End copied from covlk

#include <string.h>                    // strerror
//#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>                     // errno
//#include <ctype.h>
//#include <stdio.h>

#include "filename.hpp"
#include "filename-class.hpp"
#include "efstream.hpp"
#include "xfile.hpp"

#include "containers/array.hpp" // GrowArray
#include "containers/foreach.hpp"
#include "containers/output.hpp"
#include "containers/hash_map.hpp"
#include "containers/hash-string.hpp"            // hash<string>
#include "exceptions/xsystem.hpp"      // get_last_OS_error_string
#include "exceptions/user-error.hpp"      // XUserError
#include "debug/output.hpp"
#include "macros.hpp"                  // STATICDEF
#include "system/is_windows.hpp"
#include "system/platform.hpp"         // win_error_string
#include "text/iostream.hpp"
#include "text/line-stream.hpp"
#include "text/sstream.hpp"
#include "text/stream-utils.hpp"       // copy_stream
#include "text/string-list.hpp"
#include "text/string-utils.hpp"
#include "text/stringb.hpp"            // stringb
#include "file/file-operations.hpp"              // FileStats

#if 0
// XXX: If the name check is not exactly "native", boost won't parse \ as directory delimiters...

// A namecheck that ignores a trailing "."
static bool windows_name_check(const string &filename) {
    int n = filename.size();
    if(n && filename[n - 1] == '.') {
        return native(filename.substr(0, n - 1));
    } else
        return native(filename);
}
#endif

static bool is_initialized = false;

void init_filename() {
    if(is_initialized)
        return;
    is_initialized = true;
    // Set the default name_check to native, which means it is specfic
    // to the OS we are running on.
#if 0
    if(is_windows)
        path::default_name_check(windows_name_check);
    else
#endif
        path::default_name_check(boost::filesystem::native);

    // This gets BOOST to remember the current path as the initial path.
    initial_path();
}

struct init_filename_statically_t {
    init_filename_statically_t() {
        init_filename();
    }
};
static init_filename_statically_t init_filename_statically;

void change_dir(const path &dir) {
    // This might throw on a big file, but it would be an error anyway
    if(!exists(dir))
        COV_THROW(filesystem_error("change_directory", dir, boost::filesystem::not_found_error));
    if(!is_directory(dir))
        COV_THROW(filesystem_error("change_directory", dir, boost::filesystem::not_directory_error));
#ifdef __MC_MINGW__
    SetCurrentDirectory(dir.native_directory_string().c_str());
#else
    chdir(dir.native_directory_string().c_str());
#endif
}

path get_directory_of(const path &p) {
    path rv = p.branch_path();
    if(rv.leaf() == rv.root_name()) {
        if(p.has_root_directory()) {
            return path(p.root_name()) / "/";
        } else if(p.has_root_name()) {
            return rv;
        } else {
            return ".";
        }
    }
    return rv;
}

bool file_exists(const path &f) {
#ifdef HAVE_STAT64
    struct stat64 sb;
    int rv = stat64(f.native_file_string().c_str(), &sb);
    if(rv != 0) {
        if(errno == ENOENT || errno == ENOTDIR) {
            return false;
        }
        throw_XSystem("stat", f.native_file_string());
    }
    return !S_ISDIR(sb.st_mode);
#else
    return exists(f) && !is_directory(f);
#endif
}

bool dir_exists(const path &d) {
#ifdef HAVE_STAT64
    struct stat64 sb;
    int rv = stat64(d.native_file_string().c_str(), &sb);
    if(rv != 0) {
        if(errno == ENOENT || errno == ENOTDIR) {
            return false;
        }
        throw_XSystem("stat", d.native_file_string());
    }
    return S_ISDIR(sb.st_mode);
#else
    return exists(d) && is_directory(d);
#endif
}

bool file_exists(const char *s) {
    try {
        path p = make_path(s);
        return file_exists(p);
    } catch(filesystem_error &e) {
        COV_CAUGHT(e);
        return false;
    }
}

bool file_exists(const string &s) {
    return file_exists(s.c_str());
}

bool dir_exists(const char *s) {
    try {
        path p = make_path(s);
        return dir_exists(p);
    } catch(filesystem_error &e) {
        COV_CAUGHT(e);
        return false;
    }
}

bool dir_exists(const string &s) {
    return dir_exists(s.c_str());
}

bool symlink_exists(const path &f) {
    return symbolic_link_exists(f);
}

void remove_file_ignore_errors(const path &f)
{
    try {
        boost::filesystem::remove(f);
    } catch (boost::filesystem::filesystem_error &e) {
        COV_CAUGHT(e);
        // ignore it
    }
}


// ------------------------- path_push_t ----------------------------
path_push_t::path_push_t()
  : old_pwd(current_path()) 
{}

path_push_t::path_push_t(const path &new_dir)
  : old_pwd(current_path()) 
{
    change_dir(new_dir);
}

path_push_t::~path_push_t() 
{
    change_dir(old_pwd);
}

// ---------------------- end: path_push_t --------------------------

namespace boost { namespace filesystem {
ostream& operator<<(ostream& out, const path &p)
{
    return out << p.native_file_string();
}
}}

bool same_file(const path &p1, const path &p2)
{
#ifndef __MC_MINGW__
    return path_without_symlinks(p1).string()
        == path_without_symlinks(p2).string();
#else
    return lowerCased(path_without_symlinks(p1).string())
        == lowerCased(path_without_symlinks(p2).string());
#endif
}

// is_subdirectory
//
// Return true if "p1" is contained in "p2".
//
bool is_subdirectory(const path& p1, const path& p2)
{
    path p1comp(path_without_symlinks(p1));
    path p2comp(path_without_symlinks(p2));

    LET(p1_it, p1comp.begin());
    LET(p2_it, p2comp.begin());

    for(; p1_it != p1comp.end() && p2_it != p2comp.end();
        ++p1_it, ++p2_it) {
        string p1_str(*p1_it);
        string p2_str(*p2_it);
        if(is_windows) {
            makeLowerCase(p1_str);
            makeLowerCase(p2_str);
        }

        if (p1_str != p2_str) {
            return false;
        }
    }

    if (p1_it != p1comp.end() &&
        p2_it == p2comp.end()) {
        // p1 is longer than p2 and, thus, less than p2 because all of
        // the components are equal, it's just that p1 is underneath
        // of p2.
        return true;
    }

    // In this case, the paths are equal.  Thus, p1 is not less than p2.
    return false;
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
static bool redirect_output
(const path &fpath,
 bool from_stdout,
 bool from_stderr)
{
    string filename = fpath.native_file_string();

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

bool redirect_all_output(const path &fpath) {
    return redirect_output
        (fpath,
         /*from_stdout*/true,
         /*from_stderr*/true);
}

bool redirect_stderr(const path& fpath)
{
    return redirect_output
        (fpath,
         /*from_stdout*/false,
         /*from_stderr*/true);
}
    
bool redirect_stdout(const path& fpath)
{
    return redirect_output
        (fpath,
         /*from_stdout*/true,
         /*from_stderr*/false);
}


// Caching for realpath.  Absolutely necessary for cases like Oracle
// where symlinks are used extensively and performance of the emitter
// is catastrophic if this caching is not done.
#if (defined(PATH_MAX) && !defined(__MC_MINGW__)) || (defined(__MC_MINGW__) && defined(MAX_PATH))
typedef hash_map<string, path> canon_type_t;
static canon_type_t canonical_names(8192);

const path& get_real_path(const path &p) {
    if(p.empty())
        return p;
    LET(i, canonical_names.find(p.string()));
    if(i == canonical_names.end()) {
#if !defined(__MC_MINGW__)
        path parent( get_real_path(p.branch_path()) );
        path canon( parent / p.leaf() );
        char buf[PATH_MAX];
        int rv = readlink(canon.native_file_string().c_str(), buf, PATH_MAX-1);
        if(rv > 0) {
            buf[rv] = '\0';
            // Do not normalize the argument to get_real_path; it
            // may lose you symlinks in the parent path.
            canon = get_real_path(complete(path(buf), parent));
        }
#else
        char buf[MAX_PATH];
        path canon;
        // First get full path name
        if (!GetFullPathName(p.native_file_string().c_str(), MAX_PATH, buf, /*lpFilePart*/0)) {
            // win_error is defined in "system", which depends on this
            // library *sigh*
            // win_error("GetFullPathName");
            // Ignore the situation silently, try another way.
            // It's unlikely to fail anyway.
            canon = complete(p);
        } else
            canon = buf;
        // Then get "long" name
        if (!GetLongPathName(canon.native_file_string().c_str(), buf, MAX_PATH)) {
            // Probably means the file doesn't exist. Leave it untouched.
        } else
            canon = buf;
        // Now lowercase drive letter, for consistency
        if(canon.has_root_name()) {
            canon = path(lowerCased(canon.root_name())) / "/" / canon.relative_path();
        }
        // The file names are *still* not canonical on windows,
        // because of case-insensitivity.
        // I'd rather not lowercase here, as it makes the path looks
        // ugly. Also there may be case-sensitive filesystems.
#endif
        // This is safe to normalize because the "canon" path has no
        // symlinks in it.
        canon = complete(canon).normalize();
        string key = p.string();
        pair<canon_type_t::iterator,bool> rtn =
            canonical_names.insert(make_pair(key, canon));
        i = rtn.first;
    }
   return i->second;
}
#else
const path& get_real_path(const path &p) {
    return p;
}
#endif

// Return a path without symlinks
path path_without_symlinks(const char *a_path) {
    return get_real_path(complete(make_path(a_path)));
}

path path_without_symlinks(const string &a_path) {
    return get_real_path(complete(make_path(a_path)));
}

// Return a path without symlinks
path path_without_symlinks(const path &a_path)
{
    return get_real_path(complete(a_path));
}

string remove_double_slash(const string& file_str)
{
    string out_str;
    out_str.reserve(file_str.size());

    bool saw_slash = false;
    foreach(it, file_str) {
        if ((*it) == '/') {
            if (saw_slash) {
                continue;
            } else {
                saw_slash = true;
            }
        } else {
            saw_slash = false;
        }
        out_str += (*it);
    }
    return out_str;
}

path make_relative(const path &p0, const path &to0) {
#ifndef __MC_MINGW__
    path p(path_without_symlinks(p0)), to(path_without_symlinks(to0));
#else
    path p(lowerCased(path_without_symlinks(p0).string())), to(lowerCased(path_without_symlinks(to0).string()));
#endif
    if(p.root_name() != to.root_name()) {
        return p;
    }
    path::iterator i1 = p.begin(),
        i2 = to.begin();
    int common_count = 0;
    for(; i1 != p.end() && i2 != to.end() ; ++i1, ++i2) {
        if(*i1 != *i2) {
            break;
        }
        ++common_count;
    }
    if(common_count < 2) // If only "/" in common, no point in doing all this
        return p;
    path rel_path = ".";
    for(; i2 != to.end(); ++ i2)// Add the right number of ..
        rel_path /= "..";
    for(; i1 != p.end(); ++i1)
        rel_path /= *i1;
    return rel_path;
}

#ifdef __MC_MINGW__
string get_windows_short_name(const path& long_path)
{
    DWORD short_size = 1024;
    
    GrowArray<char> short_path(short_size);

    path p = complete(long_path).normalize();
    
    DWORD short_path_len = GetShortPathName(p.native_file_string().c_str(),
                                            short_path.getArrayNC(),
                                            short_size);

    if(short_path_len == 0) {
        return string();
    }
    if (short_path_len > short_size) {
        short_size = short_path_len;
        short_path.ensureAtLeast(short_size);
        short_path_len = GetShortPathName(p.native_file_string().c_str(),
                                          short_path.getArrayNC(),
                                          short_size);
        if(short_path_len == 0) {
            return string();
        }
    }

    return short_path.getArray();
}
#endif

const directory_iterator dir_end;
const ordered_directory_iterator ordered_dir_end;

pair<directory_iterator, directory_iterator> get_dir_entries_as_pair_t::operator()(const path &dir) const {
    directory_iterator dir_start;
    // Do not follow symlinks. Otherwise we may loop forever.
    FileStats buf;
    if(getLinkStats(dir.native_file_string().c_str(),
                    buf)
       &&
       buf.is_dir()) {
        try {
            dir_start = directory_iterator(dir);
        } catch(filesystem_error &e) {
            COV_CAUGHT(e);
            dir_start = dir_end;
            // Ignore error (e.g. permission denied): just skip
            // directory.
            
        }
    }
    return make_pair(dir_start, dir_end);
}

ordered_directory_iterator::ordered_directory_iterator(const path &dir) {
    foreach(i, dir_entries(dir)) {
        dirs.insert(i->string());
    }
}

ordered_directory_iterator &ordered_directory_iterator::operator++() {
    assert(!dirs.empty());
    dirs.erase(dirs.begin());
    return *this;
}

const string &ordered_directory_iterator::operator*() const {
    return dirs.begin().operator*();
}

const string *ordered_directory_iterator::operator->() const {
    return dirs.begin().operator->();
}

bool ordered_directory_iterator::operator==(const ordered_directory_iterator &it) const {
    return dirs == it.dirs;
}

bool ordered_directory_iterator::operator!=(const ordered_directory_iterator &it) const {
    return dirs != it.dirs;
}

// convert_dos_filename_to_unix: unicise a filename.
string convert_dos_filename_to_unix(const string& str)
{
    // Lowercase name
    string out = lowerCased(str);
    // Remove drive letter and ':'
    LET(p, str.find(':'));
    if(p != string::npos) {
        out = out.substr(p + 1);
    }

    // Now change every "\" to a "/".
    foreach(it, out) {
        if ((*it) == '\\') {
            (*it) = '/';
        }
    }

    return out;
}

/**
 * Helper class for sanitize_path that breaks up
 * and sanitizes elements of a given path.
 */
class PathSanitizer
{
    /** The input string. */
    string input_;

    /** Our computed result. */
    string output_;

    /** True if we're running in windows mode. */
    bool windows_mode_;

    /**
     * Test whether the given character is a directory separator.
     */
    bool isSeparator(char c) const
    {
        return (windows_mode_ && c == '\\') || c == '/';
    }

    /**
     * Test whether we should attempt to remove the given character if it
     * occurs at the end of a directory entry.
     */
    bool shouldSanitizeAtEnd(char c) const
    {
        return windows_mode_ && (c == '.' || c == ' ');
    }

    /**
     * Test whether we should attempt to remove the given character regardless
     * of where it occurs in the input.
     */
    bool shouldSanitize(char c) const
    {
        return windows_mode_ && c == '"';
    }

    /**
     * Append the given character to the given string if it is not
     * a character we sanitize unconditionally.
     */
    void maybeAppend(char c, string& output)
    {
        if (!shouldSanitize(c))
            output.push_back(c);
    }

    /**
     * Attempt to sanitize a sequence characters starting at \c from in the
     * string given by \c [begin,end) if the sequence occurs at the end and
     * doesn't constitute the entire string. Returns the iterator to the first
     * character (if any) after the sequence.
     */
    string::const_iterator sanitizeAtEnd(
        const string::const_iterator begin,
        const string::const_iterator from,
        const string::const_iterator end)
    {
        string::const_iterator it = from;
        string dropped;
        while (it != end && (shouldSanitize(*it) || shouldSanitizeAtEnd(*it)))
        {
            maybeAppend(*it, dropped);
            ++it;
        }

        // If the sequence starts at the beginning of the entry,
        // we don't sanitize it.
        if (begin == from || (it != end && !isSeparator(*it)))
        {
            output_.append(dropped);
        }
        return it;
    }

    /**
     * Scan through the directory entry beginning with \c begin and ending at or
     * before \c end, sanitizing forbidden character sequences as appropriate.
     * Returns the iterator to the first character (if any) after the end of the
     * entry.
     */
    string::const_iterator scanEntry(
        const string::const_iterator begin,
        const string::const_iterator end)
    {
        string::const_iterator it = begin;
        while (it != end && !isSeparator(*it))
        {
            if (shouldSanitizeAtEnd(*it))
            {
                it = sanitizeAtEnd(begin, it, end);
            }
            else
            {
                maybeAppend(*it, output_);
                ++it;
            }
        }

        return it;
    }

public:
    PathSanitizer(const string& path, bool windows_mode = is_windows) :
        input_(path),
        windows_mode_(windows_mode)
    {
        if (input_.empty() || input_ == ".")
        {
            output_ = input_;
            return;
        }

        // Handle windows UNC paths.
        if (isSeparator('\\') && begins_with(input_, "\\\\"))
            output_.push_back('\\');

        string::const_iterator it = input_.begin();
        const string::const_iterator end = input_.end();
        while (it != end)
        {
            if (isSeparator(*it))
            {
                output_.push_back(*it);
                ++it;

                while (it != end && isSeparator(*it))
                {
                    ++it;
                }
            }
            else
            {
                it = scanEntry(it, end);
            }
        }
    }

    /**
     * Convert the sanitized path into a string.
     */
    operator string() const
    {
        return output_;
    }

};

string sanitize_path(const string &filename) {
    return PathSanitizer(filename);
}

path make_path(const string &filename) {
  return path(sanitize_path(filename));
}

static void throw_last_OS_error_boost_exn(const string &who, const path &p) {
    COV_THROW(boost::filesystem::filesystem_error(who, p, get_last_OS_error_string()));
}

void set_file_writable(const path &file_name) {
#ifdef __MC_MINGW__
    // SetFileAttributes is vaild for directories as well and the only
    // pertinent attribute seems to be (the lack of) read-only for
    // both files and directories
    
    if(!SetFileAttributes(file_name.native_file_string().c_str(),
                          GetFileAttributes(file_name.native_file_string().c_str()) & (~FILE_ATTRIBUTE_READONLY))) {
        throw_last_OS_error_boost_exn("SetFileAttributes", file_name);
    }
#else

    // Do not attempt to make symlinks writable.
    struct stat stats;
    if(lstat(file_name.native_file_string().c_str(), &stats)) {
        throw_last_OS_error_boost_exn("lstat", file_name);
    }

    if(S_ISLNK(stats.st_mode))
        return;

    // For directories, all set the read/execute bits because we will
    // likely want to read the directory entries if we want to write
    // to it
    mode_t mode = stats.st_mode | S_IWUSR;
    if (S_ISDIR(stats.st_mode)) {
        mode |= S_IRUSR | S_IXUSR;
    }
    
    if(chmod(file_name.native_file_string().c_str(),mode)) {
        throw_last_OS_error_boost_exn("chmod", file_name);
    }
#endif
}

void set_file_read_only(const path &file_name) {
#ifdef __MC_MINGW__
    if(!SetFileAttributes(file_name.native_file_string().c_str(),
                          GetFileAttributes(file_name.native_file_string().c_str()) | FILE_ATTRIBUTE_READONLY)) {
        throw_last_OS_error_boost_exn(__FUNCTION__, file_name);
    }
#else
    struct stat stats;
    if(lstat(file_name.native_file_string().c_str(), &stats)) {
        throw_last_OS_error_boost_exn("lstat", file_name);
    }

    if(S_ISLNK(stats.st_mode))
        return;

    if(chmod(file_name.native_file_string().c_str(),
             stats.st_mode & (~S_IWUSR))) {
        throw_last_OS_error_boost_exn("chmod", file_name);
    }
#endif
}

void force_remove(const path &file_name) {
    if (file_exists(file_name)) {
        set_file_writable(file_name);
        remove(file_name);
    }
}

void force_remove_all(const path &dir_name) {
    FileStats buf;
    if(getLinkStats(dir_name.native_file_string().c_str(),
                    buf)
       &&
       buf.is_dir()) {
        set_file_writable(dir_name);    
        foreach(i, recursive_dir_entries(dir_name)) {
            set_file_writable(*i);
        }
        remove_all(dir_name);
    }
}

bool check_writeable(const path& p) {
    try {
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
        
        path tmp_path = p / unique_string_stream.str();
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
        } catch(file_open_error &e) {
            COV_CAUGHT(e);
            // It doesn't hurt to remove here either.
            remove(tmp_path);
            return false;
        }
    } catch(filesystem_error& e) {
        COV_CAUGHT(e);
        // Do nothing - exception means we return false.
    }
    return false;
}

namespace debug {
    // XXX TODO: Put this in a proper place
    /*extern*/ bool locks;
}

bool atomic_rename(const path &from_file, const path &to_file) {
#ifdef __MC_MINGW__
    try {
        rename(from_file, to_file);
        return true;
    } catch(filesystem_error &e) {
        COV_CAUGHT(e);
        return false;
    }
#else

    const string &from_string = from_file.native_file_string();
    const string &to_string = to_file.native_file_string();

    const char *from = from_string.c_str();
    const char *to = to_string.c_str();
    
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


// ----------------------- e[io]fstream ---------------------
static file_open_error::open_mode_t iosmode_to_open_mode_t(std::ios_base::openmode m)
{
    if (m & std::ios_base::out) {
        return file_open_error::OM_WRITE;
    }
    else {
        return file_open_error::OM_READ;
    }
}


#if 0
eifstream::eifstream(const path &p,
                   std::ios_base::openmode mode):
    std::ifstream(p.native_file_string().c_str(), mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

eifstream::eifstream(const Filename &p,
                   std::ios_base::openmode mode):
    std::ifstream(p.toString().c_str(), mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p.toString(), iosmode_to_open_mode_t(mode));
    }
}

eifstream::eifstream(const string &p,
                   std::ios_base::openmode mode):
    std::ifstream(p.c_str(), mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

eifstream::eifstream(const char *p,
                   std::ios_base::openmode mode):
    std::ifstream(p, mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

void eifstream::open(const path &p,
                    std::ios_base::openmode mode) {
    this->open(p.native_file_string(), mode);
}

void eifstream::open(const Filename &p,
                    std::ios_base::openmode mode) {
    this->open(p.toString(), mode);
}

void eifstream::open(const string &p,
                     std::ios_base::openmode mode) {
    this->open(p.c_str(), mode);
}

void eifstream::open(const char *p,
                    std::ios_base::openmode mode) {
    std::ifstream::open(p, mode);
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

eofstream::eofstream(const path &p,
                   std::ios_base::openmode mode):
    std::ofstream(p.native_file_string().c_str(), mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

eofstream::eofstream(const Filename &p,
                   std::ios_base::openmode mode):
    std::ofstream(p.toString().c_str(), mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p.toString(), iosmode_to_open_mode_t(mode));
    }
}

eofstream::eofstream(const string &p,
                   std::ios_base::openmode mode):
    std::ofstream(p.c_str(), mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

eofstream::eofstream(const char *p,
                   std::ios_base::openmode mode):
    std::ofstream(p, mode) {
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}

void eofstream::open(const path &p,
                    std::ios_base::openmode mode) {
    this->open(p.native_file_string(), mode);
}

void eofstream::open(const Filename &p,
                    std::ios_base::openmode mode) {
    this->open(p.toString(), mode);
}

void eofstream::open(const string &p,
                    std::ios_base::openmode mode) {
    this->open(p.c_str(), mode);
}

void eofstream::open(const char *p,
                    std::ios_base::openmode mode) {
    std::ofstream::open(p, mode);
    if(fail()) {
        file_open_error::throw_last_OS_file_error(p, iosmode_to_open_mode_t(mode));
    }
}
#endif

void check_open_file_RACECOND(const path &p,
                              std::ios_base::openmode mode) {
    int unix_flags = 0;
#ifdef O_LARGEFILE
    unix_flags |= O_LARGEFILE;
#endif
    if(mode & std::ios_base::out) {
        unix_flags |= O_APPEND | O_CREAT;
        if(mode & std::ios_base::in) {
            unix_flags |= O_RDWR;
        } else {
            unix_flags |= O_WRONLY;
        }
    } else {
        unix_flags |= O_RDONLY;
    }
    int fd = open(p.native_file_string().c_str(), unix_flags, 0666);
    if(fd == -1) {
        file_open_error::throw_last_OS_file_error(
                                    Filename(p, Filename::FROM_BOOST_PATH),
                                    iosmode_to_open_mode_t(mode));
    } else {
        close(fd);
    }
}


bool is_valid_filename(const string &name) {
    try {
        make_path(name);
        return true;
    } catch(filesystem_error &e) {
        COV_CAUGHT(e);
        return false;
    }
}

void get_file_lines(const Filename& file_name, list<string> &out)
{
    line_stream lstr(out);
    // line_stream should work whether text mode translation happens
    // or not. Since doing that translation takes extra work, don't do
    // it.
    eifstream in(file_name, eifstream::binary);
    copy_stream(lstr, in);
}

void get_file_lines(const path& file_name, list<string> &out)
{
    get_file_lines(Filename(file_name, Filename::FROM_BOOST_PATH,
                            SSE_SYSTEM_DEFAULT), out);
}

void do_touch(const path &p) {
    if(!file_exists(p)) {
        eofstream of(p, eofstream::text);
        of << endl;
    }
}

void copy_files(const path &from, const path &to) {
    if (!dir_exists(from)) {
        COV_THROW(filesystem_error("copy_files",
                               from, boost::filesystem::not_directory_error));
    }
    create_directories(to);

    foreach(it, dir_entries(from)) {
        if (dir_exists(*it)) {
            copy_files(from / it->leaf(), to / it->leaf());
        } else {
            copy_file(from / it->leaf(), to / it->leaf());
        }
    }
}

// dchai: url_file_name belongs in utilities/cov-format-errors.  We
// should just construct Windows paths from the get-go.
string url_file_name(const string &file) {
    string rv;
    if(is_windows) {
        foreach(i, file) {
            if(*i == '\\')
                rv += '/';
            else
                rv += *i;
        }
    } else {
        rv = file;
    }
    return rv;
}

string url_file_name(const path &p) {
    return url_file_name(p.native_file_string());
}


string read_whole_text_file(const Filename &fname)
{
    eifstream in(fname, eifstream::text);
    return read_whole_stream(in);
}

string read_whole_text_file(const path &fname)
{
    return read_whole_text_file(Filename(fname, Filename::FROM_BOOST_PATH,
                                                SSE_SYSTEM_DEFAULT));
}

string read_whole_binary_file(const Filename &fname)
{
    eifstream in(fname, eifstream::binary);
    return read_whole_stream(in);
}

string read_whole_binary_file(const path &fname)
{
    return read_whole_binary_file(Filename(fname, Filename::FROM_BOOST_PATH,
                                                  SSE_SYSTEM_DEFAULT));
}

void write_data_to_new_file(const Filename &filename, const string &data)
{
    eofstream out(
        filename,
        OPENFLAGS_WRITE |
        OPENFLAGS_ALLOW_CREATE |
        OPENFLAGS_TRUNCATE |
        OPENFLAGS_BINARY);
    out << data;
}

// dchai: these should probably go into libs/system
bool path_is_case_sensitive(const char *fname)
{
#ifdef __MC_MINGW__
  return false;
#else
  return true;
#endif
}

bool dir_is_case_sensitive(const char *fname)
{
#ifdef __MC_MINGW__
  return false;
#else
  return true;
#endif
}

void split_path(const char *path, string &dname, string &fname)
{
  const char *fnp=path+strlen(path);
  for (;;) {
    if (fnp == path) {
      fname=path;
      break;
    }
    else if (*fnp == '\\' || *fnp == '/') { 
      fname = fnp+1;
      dname.append(path,fnp-path);
      break;
    }
    
    --fnp;
  }
}

#ifdef __MC_MINGW__
bool make_cygwin_symlink(const char *path, const char* link)
{
    {
        ofstream to(path);
        to << "!<symlink>";
        to << link;
        to.put('\0');
    }
    if (!SetFileAttributes(path, FILE_ATTRIBUTE_SYSTEM)) {
        return false;
    }
    return true;
}
#endif

bool is_absolute_windows_path(const string &p)
{
    /*
     * Path is absolute if it begins with a drive letter or a net path \\
    */
    return (p.length() >= 2 &&
            (p.at(1) == ':' || 
             (p.at(0) == '\\' &&
              p.at(1) == '\\')));
}

/**
 * A utility class for defining test cases for the sanitize_path function.
 */
class SanitizePathTest
{
    string input_;
    string expected_;

    /** Prints out useful information when we fail to sanitize a path. */
    void failure(
        const string& message,
        const string& input,
        const string& output,
        const string& expected) const
    {
        ostringstream stream;

        stream << message << endl;
        stream << "Input path: " << input << endl;
        stream << "Sanitized path: " << output << endl;
        stream << "Expected path: " << expected;

        xfailure(stream.str());
    }

    /**
     * Run the test.
     */
    void runTest() const
    {
        const string sanitized = sanitize_path(input_);
        if (sanitized != expected_)
        {
            failure(
                "Sanitized path does not match expected result:",
                input_, sanitized, expected_);
        }

        try
        {
            boost::filesystem::path(sanitized);
        }
        catch (const filesystem_error&)
        {
            failure("Failed to sanitize path:", input_, sanitized, expected_);
        }
    }

public:
    /**
     * Finalize and run a test case.
     */
    ~SanitizePathTest()
    {
        runTest();
    }

    /**
     * Set the input for the text.
     */
    SanitizePathTest& input(const string& input)
    {
        cond_assert(input_.empty());
        input_ = input;
        return *this;
    }

    /**
     * Set the expected output for the text.
     */
    SanitizePathTest& output(const string& result)
    {
        expected_ = result;
        return *this;
    }

    /**
     * Set the expected output for the test to be the same as the input.
     */
    SanitizePathTest& unchanged()
    {
        return output(input_);
    }

    /**
     * Set the expected output when running on Windows or Cygwin.
     */
    SanitizePathTest& windowsOutput(const string& result)
    {
        if (is_windows) expected_ = result;
        return *this;
    }

    /**
     * Set the expected output when running on a Unix system.
     */
    SanitizePathTest& unixOutput(const string& result)
    {
        if (!is_windows) expected_ = result;
        return *this;
    }
};

void sanitize_path_tests()
{
    SanitizePathTest()
        .input("")
        .unchanged()
    ;

    SanitizePathTest()
        .input(".")
        .unchanged()
    ;

    SanitizePathTest()
        .input("..")
        .unchanged()
    ;

    SanitizePathTest()
        .input("/")
        .unchanged()
    ;

    SanitizePathTest()
        .input("./")
        .unchanged()
    ;

    SanitizePathTest()
        .input("../foo/..")
        .unchanged()
    ;

    SanitizePathTest()
        .input("..\\foo\\..")
        .unchanged()
    ;

    SanitizePathTest()
        .input("./.foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input("./foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input(".\\.foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input(".\\foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input("//foo///bar//baz")
        .output("/foo/bar/baz")
    ;

    SanitizePathTest()
        .input("//mach//home//t.cpp")
        .output("/mach/home/t.cpp")
    ;

    SanitizePathTest()
        .input("/foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input("/foo.")
        .windowsOutput("/foo")
        .unixOutput("/foo.")
    ;

    SanitizePathTest()
        .input("/foo/////bar")
        .output("/foo/bar")
    ;

    SanitizePathTest()
        .input("/foo//bar")
        .output("/foo/bar")
    ;

    SanitizePathTest()
        .input("/foo/bar")
        .unchanged()
    ;

    SanitizePathTest()
        .input("/usr//inc//t.cpp")
        .output("/usr/inc/t.cpp")
    ;

    SanitizePathTest()
        .input("\\\\mach\\home\\\\t.cpp")
        .windowsOutput("\\\\mach\\home\\t.cpp")
        .unixOutput("\\\\mach\\home\\\\t.cpp")
    ;

    SanitizePathTest()
        .input("C:\\\\foo\\\\\\bar\\\\baz")
        .windowsOutput("C:\\foo\\bar\\baz")
        .unixOutput("C:\\\\foo\\\\\\bar\\\\baz")
    ;

    SanitizePathTest()
        .input("C:\\dir\\\\t.cpp")
        .windowsOutput("C:\\dir\\t.cpp")
        .unixOutput("C:\\dir\\\\t.cpp")
    ;

    // BZ 60690
    SanitizePathTest()
        .input("\\notUNC\\path")
        .unchanged()
    ;

    SanitizePathTest()
        .input("C:\\foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input("C:\\foo.")
        .windowsOutput("C:\\foo")
        .unixOutput("C:\\foo.")
    ;

    SanitizePathTest()
        .input("C:\\foo..")
        .windowsOutput("C:\\foo")
        .unixOutput("C:\\foo..")
    ;

    SanitizePathTest()
        .input("C:\\foo.bar")
        .unchanged()
    ;

    SanitizePathTest()
        .input("C:\\foo\\\\\\\\bar")
        .windowsOutput("C:\\foo\\bar")
        .unixOutput("C:\\foo\\\\\\\\bar")
    ;

    SanitizePathTest()
        .input("C:\\foo\\\\bar")
        .windowsOutput("C:\\foo\\bar")
        .unixOutput("C:\\foo\\\\bar")
    ;

    SanitizePathTest()
        .input("C:\\foo\\bar")
        .unchanged()
    ;

    SanitizePathTest()
        .input("foo ")
        .windowsOutput("foo")
        .unixOutput("foo ")
    ;

    SanitizePathTest()
        .input("foo \\bar ")
        .windowsOutput("foo\\bar")
        .unixOutput("foo \\bar ")
    ;

    SanitizePathTest()
        .input("foo")
        .unchanged()
    ;

    SanitizePathTest()
        .input("foo.")
        .windowsOutput("foo")
        .unixOutput("foo.")
    ;

    SanitizePathTest()
        .input("foo..")
        .windowsOutput("foo")
        .unixOutput("foo..")
    ;

    SanitizePathTest()
        .input("foo.bar")
        .unchanged()
    ;

    SanitizePathTest()
        .input("foo/////bar")
        .output("foo/bar")
    ;

    SanitizePathTest()
        .input("foo//bar")
        .output("foo/bar")
    ;

    SanitizePathTest()
        .input("foo/bar")
        .unchanged()
    ;

    SanitizePathTest()
        .input("foo/bar./baz")
        .windowsOutput("foo/bar/baz")
        .unixOutput("foo/bar./baz")
    ;

    SanitizePathTest()
        .input("foo\\\\\\\\\\")
        .windowsOutput("foo\\")
        .unixOutput("foo\\\\\\\\\\")
    ;

    SanitizePathTest()
        .input("foo\\bar")
        .unchanged()
    ;

    SanitizePathTest()
        .input("foo\\bar.\\baz")
        .windowsOutput("foo\\bar\\baz")
        .unixOutput("foo\\bar.\\baz")
    ;

    SanitizePathTest()
        .input("foo\\bar\"\\\"baz")
        .windowsOutput("foo\\bar\\baz")
        .unixOutput("foo\\bar\"\\\"baz")
    ;

    SanitizePathTest()
        .input("home\\\\t.cpp")
        .windowsOutput("home\\t.cpp")
        .unixOutput("home\\\\t.cpp")
    ;

    SanitizePathTest()
        .input("//////foo")
        .output("/foo")
    ;

    SanitizePathTest()
        .input("\\\\\\foo")
        .unixOutput("\\\\\\foo")
        .windowsOutput("\\\\foo")
    ;

    SanitizePathTest()
        .input("foo.\".")
        .unixOutput("foo.\".")
        .windowsOutput("foo")
    ;

    SanitizePathTest()
        .input("foo bar\\baz ")
        .windowsOutput("foo bar\\baz")
        .unixOutput("foo bar\\baz ")
    ;

    SanitizePathTest()
        .input("foo bar/baz ")
        .windowsOutput("foo bar/baz")
        .unixOutput("foo bar/baz ")
    ;

    SanitizePathTest()
        .input("foo.bar \\baz ")
        .windowsOutput("foo.bar\\baz")
        .unixOutput("foo.bar \\baz ")
    ;

    SanitizePathTest()
        .input("foo.bar /baz ")
        .windowsOutput("foo.bar/baz")
        .unixOutput("foo.bar /baz ")
    ;
}

void filename_unit_tests(int argc, char const * const *argv) {
    if(argc > 1) {
        // I haven't really figured out what would be useful to place
        // here, but it seems like there could be something (see
        // e.g. filename_class_unit_tests)
    }
    string invalid = ":not a valid file (on Windows):";
    cond_assert(!file_exists(invalid));
    cond_assert(!dir_exists(invalid));
    cond_assert(!is_windows || !is_valid_filename(invalid));
    path not_found("not found");
    try {
        eifstream in(not_found, eifstream::binary);
        xfailure("File should not be found");
    } catch(file_does_not_exist_error &e) {
        // Good
        cond_assert(e.p == Filename(not_found, Filename::FROM_BOOST_PATH));
        cond_assert(e.mode == file_open_error::OM_READ);
    }
    try {
        change_dir(not_found);
        xfailure("Cannot chdir to non-existent directory");
    } catch(exception &e) {
        // Good
    }
    const char *testsuite_dir = getenv("TESTSUITE_DIR");
    if(!testsuite_dir) {
        cerr << "Warning: TESTSUITE_DIR not set, using /tmp" << endl;
        testsuite_dir = "/tmp";
    }
    cond_assert(get_directory_of(".") == ".");
    cond_assert(get_directory_of("rel") == ".");
    cond_assert(get_directory_of("/") == "/");
    cond_assert(get_directory_of("/foo") == "/");
    cond_assert(get_directory_of("/foo/bar") == "/foo");
    cond_assert(is_subdirectory("/foo/bar", "/foo"));
    cond_assert(is_subdirectory("/foo/bar", "/foo/"));
    cond_assert(is_subdirectory("/foo/bar", "/"));
    cond_assert(!is_subdirectory("/foo/bar", "/foo/bar"));
    cond_assert(!is_subdirectory("/foo/bar", "/bar"));
    cond_assert(!is_subdirectory("/foo/bar", "/fo"));
    cond_assert(!is_subdirectory("/foo/bar", "/foo/ba"));
    if(is_windows) {
        cond_assert(get_directory_of("c:/") == "c:/");
        cond_assert(get_directory_of("c:foo") == "c:");
        cond_assert(get_directory_of("c:foo/bar") == "c:foo");
        cond_assert(is_subdirectory("/Foo/bar", "/foo"));
        cond_assert(!is_subdirectory("d:/Foo/bar", "c:/foo"));
    }
    {
        path rpath;

        /* Normalization will remove ".." */
        rpath = "/foo/include/gcc/i386-pc-cygwin/3.4.4/"
                "../../../../include/w32api/";
        rpath = rpath.normalize();
        cond_assert(rpath=="/foo/include/w32api");

        /* Normalization will remove ".." unless leading */
        rpath = "../../inc";
        rpath = rpath.normalize();
        cond_assert(rpath=="../../inc");
    }
    if (is_windows) {
        BLOCK_CHANGE_DIR("C:/cygwin");
        path rpath = get_real_path("usr/include");
        cond_assert(str_equali(rpath.string(), "c:/cygwin/usr/include"));

#if 0
        /* Complete does not honor BLOCK_CHANGE_DIR */
        rpath=complete("usr/include");
        cerr << "rpath = " << rpath << endl;
        cond_assert(str_equali(rpath.string(), "c:/cygwin/usr/include"));

        rpath=path_without_symlinks("usr/include");
        cerr << "rpath = " << rpath << endl;
        cond_assert(str_equali(rpath.string(), "c:/cygwin/usr/include"));
#endif
    }
#if 0
    else {
        /* BLOCK_CHANGE_DIR does not work */
        BLOCK_CHANGE_DIR("/");
        path rpath = get_real_path("bin");
        cond_assert(rpath=="/bin");

        rpath=complete("bin");
        cond_assert(rpath=="/bin");

        rpath=path_without_symlinks("bin");
        cond_assert(rpath=="/bin");
    }
#endif
    path test_dir(testsuite_dir);
    test_dir /= "filename-test";
    cond_assert(check_writeable(test_dir));
    path test_file(test_dir / "test-file");
    Filename test_file2(test_file.string(), Filename::FI_PORTABLE, SSE_SYSTEM_DEFAULT);
    try {
        string test = read_whole_binary_file(test_file);
        xfailure("File should not exist");
    } catch(file_does_not_exist_error &) {
        // Expected
    }
    try {
        eofstream out(test_dir, eofstream::binary);
        xfailure("Shouldn't be able to open a directory");
    } catch(file_does_not_exist_error &) {
        xfailure("Not a file doesn't exist error");
    } catch(XUserError &e) {
        cout << "Got expected user error: " << e.what_short()
             << endl;
    }
    {
        eofstream test(test_dir / "test", eofstream::text);
        test << "Hello";
    }
    try {
        eifstream in(test_dir, eifstream::binary);
        xfailure("Shouldn't be able to open a directory");
    } catch(file_does_not_exist_error &) {
        xfailure("Not a file doesn't exist error");
    } catch(XUserError &e) {
        cout << "Got expected user error: " << e.what_short()
             << endl;
    }
    {
        eofstream out(test_file, eofstream::text);
        out << "Hello" << endl;
        out.close();
        out.open(test_file, eofstream::text);
        out << "Hello" << endl;
    }
    {
        eofstream out(test_file2, eofstream::text);
        out << "Hello" << endl;
        out.close();
        out.open(test_file2, eofstream::text);
        out << "Hello" << endl;
    }
    {
        eofstream out(test_file.native_file_string(), eofstream::text);
        out << "Hello" << endl;
        out.close();
        out.open(test_file.native_file_string(), eofstream::text);
        out << "Hello" << endl;
    }
    {
        eofstream out(test_file.string(), eofstream::text);
        out << "Hello" << endl;
        out.close();
        out.open(test_file.string(), eofstream::text);
        out << "Hello" << endl;
    }
    {
        eofstream out(test_file.string().c_str(), eofstream::text);
        out << "Hello" << endl;
        out.close();
        out.open(test_file.string().c_str(), eofstream::text);
        out << "Hello" << endl;
    }
    {
        string test = read_whole_binary_file(test_file);
#ifdef __MC_MINGW__
        #define EOL "\r\n"
#else
        #define EOL "\n"
#endif
        cond_assert(test == "Hello" EOL);
    }
    {
        eifstream in(test_file, eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
        in.close();
        in.open(test_file, eifstream::text);
        test.clear();
        in >> test;
        cond_assert(test == "Hello");
    }
    {
        eifstream in(test_file2, eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
        in.close();
        in.open(test_file2, eifstream::text);
        test.clear();
        in >> test;
        cond_assert(test == "Hello");
    }
    {
        eifstream in(test_file.native_file_string(), eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
        in.close();
        in.open(test_file.native_file_string(), eifstream::text);
        test.clear();
        in >> test;
        cond_assert(test == "Hello");
    }
    {
        eifstream in(test_file.string(), eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
        in.close();
        in.open(test_file.string(), eifstream::text);
        test.clear();
        in >> test;
        cond_assert(test == "Hello");
    }
    {
        eifstream in(test_file.string().c_str(), eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
        in.close();
        in.open(test_file.string().c_str(), eifstream::text);
        test.clear();
        in >> test;
        cond_assert(test == "Hello");
    }
    {
        eifstream in(test_file.string().c_str(), eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
        in.close();
        in.open(test_file.string().c_str(), eifstream::text);
        test.clear();
        in >> test;
        cond_assert(test == "Hello");
    }
    set_file_read_only(test_file);
    try {
        eofstream out(test_file, eofstream::text);
        xfailure("Shouldn't be able to open a read-only file for writing");
    } catch(file_does_not_exist_error &) {
        xfailure("Not a file doesn't exist error");
    } catch(file_open_error &e) {
        cond_assert(e.p == Filename(test_file, Filename::FROM_BOOST_PATH));
        cond_assert(e.errnoCode == EACCES);
        cond_assert(e.mode == file_open_error::OM_WRITE);
    }
    {
        eifstream in(test_file, eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Hello");
    }
    set_file_writable(test_file);
    {
        eofstream out(test_file, eofstream::text);
        out << "Goodbye" << endl;
    }
    {
        eifstream in(test_file, eifstream::text);
        string test;
        in >> test;
        cond_assert(test == "Goodbye");
    }
    if(!is_windows) {
        // set_file_read_only doesn't work on Windows.
        // See
        // http://support.microsoft.com/kb/326549
        // and the doc for SetFileAttributes:
        // http://msdn.microsoft.com/en-us/library/aa365535(VS.85).aspx
        set_file_read_only(test_dir);
        cond_assert(!check_writeable(test_dir));
        cond_assert(!check_writeable(test_dir / "foo"));
        cond_assert(!atomic_rename(test_file, test_dir / "test2"));
        remove_file_ignore_errors(test_file);
        cond_assert(file_exists(test_file));
        set_file_writable(test_dir);
        cond_assert(check_writeable(test_dir));
    }

    sanitize_path_tests();

    remove_all(test_dir);
}

// EOF



