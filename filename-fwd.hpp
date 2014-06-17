/* filename-fwd.h
 *
 * Interface to the file name handling routines.  The key reason to use
 * these routines is to ensure that the file names for windows and Unix
 * are both handled cleanly and that all assumptions about path
 * delimiters and such are centralized here.  Some of the functions in
 * this interface return heap allocated memory that must be freed by the
 * callers.
 *
 * (c) 2002 The Board of Trustees of the Leland Stanford Junior University
 * (c) 2003-2011 Coverity, Inc. All rights reserved worldwide.
 */

#ifndef COV_FILENAME_FWD_HPP
#define COV_FILENAME_FWD_HPP

// this dir
#include "path-fwd.hpp"                // path

#include "text/string-fwd.hpp"
#include "text/iostream-fwd.hpp"

class eofstream;
class eifstream;

#ifdef __MSVC__
// win64 builds use latest BOOST (1.42)
namespace boost {
    namespace filesystem {
        template<class Path> class basic_directory_iterator;
        typedef basic_directory_iterator<path> directory_iterator;
        
        template <class Path> class basic_filesystem_error;
        typedef basic_filesystem_error<path> filesystem_error;     

        void copy_file(const path &, const path &);
        void rename(const path &, const path &);
        bool remove(const path &);
        unsigned long remove_all(const path &);
        bool create_directory(const path &);
        bool create_directories(const path &);
        // Print out p.native_file_string()
        // Put in namespace or it causes problems when used from
        // within another namespace
        ostream& operator<<(ostream& out, const path &p);
    }
}
#else
namespace boost {
    namespace filesystem {
        class directory_iterator;
        class filesystem_error;
        void copy_file(const path &, const path &);
        void rename(const path &, const path &);
        bool remove(const path &);
        unsigned long remove_all(const path &);
        bool create_directory(const path &);
        bool create_directories(const path &);
        // Print out p.native_file_string()
        // Put in namespace or it causes problems when used from
        // within another namespace
        ostream& operator<<(ostream& out, const path &p);
    }
}
#endif

// Import boost functions
using boost::filesystem::directory_iterator;
using boost::filesystem::filesystem_error;
using boost::filesystem::rename;
using boost::filesystem::remove;
using boost::filesystem::remove_all;
using boost::filesystem::copy_file;
using boost::filesystem::create_directory;
using boost::filesystem::create_directories;

/* Delimiter used to separate paths in a file. */
#ifdef __MC_MINGW__
#define MC_FILE_DELIM '\\'
#define MC_FILE_DELIM_STR "\\"
#else
#define MC_FILE_DELIM '/'
#define MC_FILE_DELIM_STR "/"
#endif

// Some functions are provided directly by boost:
//
// create_directory()
// create_directories()
// current_path()
// remove()
// remove_all()
//
// Watch out for exceptions when using these BOOST filesystem
// functions.
//

void init_filename();

// The following functions are not in BOOST, or provide a slightly
// cleaner interface.


/**
 * Returns the directory containing that file.
 * The difference with "branch_path" is that it will return "."
 * instead of (empty) if the file name doesn't have a directory
 **/
path get_directory_of(const path &p);
/**
 * Returns if a file exists and is not a directory. Never throws.
 **/
bool file_exists(const char *f);
/**
 * Returns if a file exists and is not a directory. Never throws.
 **/
bool file_exists(const string &f);
/**
 * Returns if a file exists and is not a directory. The conversion to
 * path might throw.
 **/
bool file_exists(const path &f);
/**
 * Returns if a file exists and is a directory. Never throws.
 **/
bool dir_exists(const char *f);
/**
 * Returns if a file exists and is a directory. Never throws.
 **/
bool dir_exists(const string &f);
/**
 * Returns if a file exists and is a directory. The conversion to
 * path might throw.
 **/
bool dir_exists(const path &f);

/**
 * Returns if a path exists and is a symbolic link.
 **/
bool symlink_exists(const path &f);

void change_dir(const path &d);

/**
 * Returns whther 2 files are the same, resolving symlinks
 **/
bool same_file(const path &p1, const path &p2);

// Redirection
bool redirect_all_output(const path &filename);
bool redirect_stdout(const path &filename);
bool redirect_stderr(const path &filename);

// is_subdirectory: return true if p1 is a sub-directory of p2.
bool is_subdirectory(const path& p1, const path& p2);

/**
 * Checks if \p p is a writable directory by trying to write to a
 * temporary file then cleaning up that file.
 * Also creates the directory if possible.
 **/
bool check_writeable(const path& p);

/**
 * Attempt to make file writable. Throws a boost filesystem exception on failure. 
 **/
void set_file_writable(const path &file_name);
/**
 * Attempt to make file non-writable. Throws a boost filesystem
 * exception on failure.
 **/
void set_file_read_only(const path &file_name);
/**
 * Attempt to make the file writable, then remove it.
 * Throws a boost filesystem exception on failure.
 **/
void force_remove(const path &file_name);
/**
 * Recursively visits all files and subdirectories, setting them
 * writable and attempting to delete them.
 * Throws a boost filesystem exception on failure.
 **/
void force_remove_all(const path &dir_name);

// SGM 2007-02-12: moved to libs/exceptions/xsystem.hpp:
//void map_last_OS_error_to_errno();

/**
 * Returns whether the given string is a valid filename for boost
 **/
bool is_valid_filename(const string &name);

/**
 * Attempts an "atomic" rename of \p from to \p to.
 * Should work even over NFS. Can be used to implement locks.
 * Note: this only works if the file has a single hard link count.
 * \return Success
 **/
bool atomic_rename(const path &from, const path &to);

/**
 * Creates file \p p if it doesn't exits, similar to unix \p touch
 * Throws boost filesystem_error if it can't create.
 **/
void do_touch(const path &);

/**
 * copy in \c from recursively to directory \p to (created if
 * necessary)
 **/
void copy_files(const path &from, const path &to);

#ifdef __MC_MINGW__
/**
 * generate a textual cygwin symbolic link. create a symlink \p path linked to \p link
 **/
bool make_cygwin_symlink(const char *path, const char* link);
#endif

#endif /* __FILENAME__H_ */
