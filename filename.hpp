/* filename.h
 *
 * Interface to the file name handling routines.  The key reason to use
 * these routines is to ensure that the file names for windows and Unix
 * are both handled cleanly and that all assumptions about path
 * delimiters and such are centralized here.  Some of the functions in
 * this interface return heap allocated memory that must be freed by the
 * callers.
 *
 * (c) 2002 The Board of Trustees of the Leland Stanford Junior University
 * (c) 2004-2014 Coverity, Inc. All rights reserved worldwide.
 */

#ifndef COV_FILENAME_HPP
#define COV_FILENAME_HPP

#include "filename-fwd.hpp"
#include "filename-class-fwd.hpp"

// The macro "R1" is incorrecly defined in some system header files on
// Solaris.  We undef it here.
#ifdef R1
#undef R1
#endif
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/exception.hpp>
#include "text/fstream.hpp"
#include "text/string.hpp"
#include "text/iostream.hpp"
#include "containers/container-utils.hpp"        // iterator_pair_wrapper_t
#include "containers/pair.hpp"
#include "containers/list.hpp"
#include "containers/set.hpp"
#include "containers/iterator.hpp"
#include "containers/recursive-iterator.hpp"
#include "exceptions/xsystem.hpp"                // XSystem
#include "macros.hpp"                            // SYMBOL_IN_MACRO

// Import boost functions returning path (not in fwd)
using boost::filesystem::complete;
using boost::filesystem::initial_path;
using boost::filesystem::current_path;
using boost::filesystem::change_extension;
using boost::filesystem::extension;
using boost::filesystem::basename;

/**
 * Returns the directory containing that file.
 * The difference with "branch_path" is that it will return "."
 * instead of (empty) if the file name doesn't have a directory
 **/
path get_directory_of(const path &p);
/**
 * Remove the file.  If there is any error, ignore it and continue.
 **/
void remove_file_ignore_errors(const path &f);

/**
 * RAII for change_dir. Allows to temporarily change directory, and come back later
 **/
class path_push_t {
public:      // funcs
    // this variant is useful if I want to come back at end of
    // scope, but do not (yet) want to change my directory
    path_push_t();

    // temporary directory change
    path_push_t(const path &new_dir);

    // restore
    ~path_push_t();

public:      // data
    // path to restore to at end of scope
    path old_pwd;
};

// Changes directory for the current scope
#define BLOCK_CHANGE_DIR(path) path_push_t SYMBOL_IN_MACRO(block_path)(path)

// Return a path without symlinks
path path_without_symlinks(const char *);
path path_without_symlinks(const string &);
// Return a path without symlinks
path path_without_symlinks(const path &);

string remove_double_slash(const string& file_str);

/**
 * Returns \p p made relative to \p to.
 * If \p p and \p to don't have the same root name, or don't have the first element in common, returns \c path_without_symlinks(p),
 * otherwise returns q such that (to / q) == p
 **/
path make_relative(const path &p, const path &to);

// convert_dos_filename_to_unix: takes a normal windows file name -
// c:\...\..\foobar.c and makes it a unix-form: strip the c:, make "\"
// to "/".
string convert_dos_filename_to_unix(const string& str);

#ifdef __MC_MINGW__
string get_windows_short_name(const path& long_path);
#endif

// A default constructed directory iterator, "past-the-end"
extern const directory_iterator dir_end;

/**
 * A wrapper around a \c path to be able to use foreach on its entries, if it's a directory
 * e.g. <tt>foreach(file_it, dir_entries(dir))</tt>
 **/
struct dir_entries_t: public iterator_pair_wrapper_t<directory_iterator> {
    dir_entries_t(const path &dir):iterator_pair_wrapper_t<directory_iterator>(directory_iterator(dir), dir_end){}
};

/**
 * A functions with a nicer name (not naming the struct this way
 * to avoid polluting the namespace with a type)
 **/
inline dir_entries_t dir_entries(const path &dir) {
    return dir_entries_t(dir);
}

/**
 * For recursive_directory_iterator
 **/
struct get_dir_entries_as_pair_t {
    pair<directory_iterator, directory_iterator> operator()(const path &dir) const;
};
/**
 * An iterator on all the files/subdirectories of a given directory (recursively)
 * Directories are visited before their contents
 **/
typedef recursive_iterator<directory_iterator, get_dir_entries_as_pair_t> recursive_directory_iterator;

struct recursive_dir_entries_t {
    recursive_dir_entries_t(const path &dir):
        dir(dir){}
    recursive_directory_iterator begin() const{
#ifdef __MSVC__
        return recursive_directory_iterator(boost::filesystem::directory_entry(dir));
#else
        return recursive_directory_iterator(dir);
#endif
    }
    recursive_directory_iterator end() const {
        return recursive_directory_iterator();
    }
private:
    const path dir;
};

/**
 * foreach(i, recursive_dir_entries(dir)) iterates over all the files / directories
 * recursively contained in dir, in depth first preorder.
 **/
inline recursive_dir_entries_t recursive_dir_entries(const path &dir) {
    return recursive_dir_entries_t(dir);
}

/**
 * Same as directory_iterator, except ordered (alphabetically)
 * Does an initial iteration and then iterates on the set of returned files
 **/
struct ordered_directory_iterator : public iterator<
    iterator_traits<directory_iterator>::iterator_category,
                                    path,
                                    iterator_traits<directory_iterator>::difference_type,
                                    const string *,
                                    const string &>
 {
public:
    ordered_directory_iterator(){}
    ordered_directory_iterator(const path &dir);
    ordered_directory_iterator &operator++();
    const string &operator*() const;
    const string *operator->() const;
    bool operator==(const ordered_directory_iterator &it) const;
    bool operator!=(const ordered_directory_iterator &it) const;
private:
    set<string> dirs;
};

extern const ordered_directory_iterator ordered_dir_end;

struct ordered_dir_entries_t: public iterator_pair_wrapper_t<ordered_directory_iterator> {
    ordered_dir_entries_t(const path &dir):
        iterator_pair_wrapper_t<ordered_directory_iterator>(ordered_directory_iterator(dir), ordered_dir_end){}
};

/**
 * foreach(i, ordered_dir_entries(dir)) iterates over all the files / directories
 * contained in dir, in alphabetical order.
 **/
inline ordered_dir_entries_t ordered_dir_entries(const path &dir) {
    return ordered_dir_entries_t(dir);
}

/**
 * For recursive_directory_iterator
 **/
struct get_ordered_dir_entries_as_pair_t {
    pair<ordered_directory_iterator, ordered_directory_iterator> operator()(const path &dir) const {
        if(dir_exists(dir))
            return make_pair(ordered_directory_iterator(dir), ordered_dir_end);
        else
            return make_pair(ordered_dir_end, ordered_dir_end);
    }
};

/**
 * An iterator on all the files/subdirectories of a given directory (recursively)
 * Directories are visited before their contents
 **/
typedef recursive_iterator<ordered_directory_iterator, get_ordered_dir_entries_as_pair_t> ordered_recursive_directory_iterator; 

struct ordered_recursive_dir_entries_t {
    ordered_recursive_dir_entries_t(const path &dir):
        dir(dir){}
    ordered_recursive_directory_iterator begin() const{
        return ordered_recursive_directory_iterator(dir);
    }
    ordered_recursive_directory_iterator end() const {
        return ordered_recursive_directory_iterator();
    }
private:
    const path dir;
};

/**
 * foreach(i, ordered_recursive_dir_entries(dir)) iterates over all the files / directories
 * contained in dir and its subdirectories, depth-first preorder, and within a directory in alphabetical order.
 **/
inline ordered_recursive_dir_entries_t ordered_recursive_dir_entries(const path &dir) {
    return ordered_recursive_dir_entries_t(dir);
}

/**
 * Sanitize paths to deal with some issues found on windows that cause 
 * problems with boost constructors, namely:
 * - trailing space
 * - trailing '.'
 * - doubled '\'
 **/
string sanitize_path(const string &filename);

/**
 * Wrapper around the boost path ctor, to sanitize path. See sanitize_path.
 **/
path make_path(const string &filename);

// SGM 2007-02-12: moved to libs/exceptions/xsystem.hpp:
//string get_last_OS_error_string();

/**
 * Throws an exception if the file can't be open with the given
 * mode. Might create the file if mode is "out".
 *
 * SGM 2006-04-13: Appending "_RACECOND" to the name because this
 * API encourages race conditions.  See DR3875 and DR3858.  Use
 * only if there is no alternative.
 **/
/* dchai: replace with call to efstream::open/close */
void check_open_file_RACECOND(const path &p,
                              ios_base::openmode mode);

// Turn a file into a list of its lines.
void get_file_lines(const Filename& file_name, list<string> &out);
void get_file_lines(const path& file_name, list<string> &out);

// Read an entire file into an in-memory string.  Will throw an
// exception if the file cannot be opened (in this way it differs from
// cat_file). Also, unlike cat_file, this reads the file in text mode.
string read_whole_text_file(const path &fname);
string read_whole_text_file(const Filename &fname);


string read_whole_binary_file(const path &fname);
string read_whole_binary_file(const Filename &fname);

// Create a new empty file and write a binary string to it.
void write_data_to_new_file(const Filename &filename, const string &data);


string url_file_name(const string &file);
string url_file_name(const path &p);


/**
 * Indicates if the path is case sensitive
 **/

bool path_is_case_sensitive(const char *fname);

/**
 * Indicates if the dir and all files in directory are case sensitive
 **/

bool dir_is_case_sensitive(const char *fname);

/**
 * Split path into directory and filename
 **/

void split_path(const char *path, string &dname, string &fname);

/**
 * Indicates if the path is absolute, on Windows this means prefixed
 * with drive letter or network path. Cygwin style paths do not count.
 **/
bool is_absolute_windows_path(const string &);

void filename_unit_tests(int argc, char const * const *argv);

#endif /* __FILENAME__H_ */
