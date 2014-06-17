/**
 * \file file-operations.hpp
 *
 * Filesystem operations, using "Filename" as argument
 *
 * Implemented in filename-class.cpp
 *
 * (c) 2011-2014 Coverity, Inc. All rights reserved worldwide.
 *
 **/
#ifndef FILE_OPERATIONS_HPP
#define FILE_OPERATIONS_HPP

#include "filename-class.hpp" // Filename

#include "libs/macros.hpp" // OPEN_NAMESPACE
#include "libs/refct-ptr.hpp"

#include <time.h> // time_t

#include <iterator> // std::iterator

#include "containers/recursive-iterator.hpp"

OPEN_NAMESPACE(FilenameNS);

// File operations on Filename
// These can be done when boost refuses the filename.

/**
 * Same as a struct stat, except more portable, since on 32-bit Linux it
 * will allow operations on files with size >= 2^31
 * This also ignores less portable information, such as access flags,
 * number of links, inode
 **/
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
    ::time_t last_access;
    ::time_t last_modification;
    unsigned long long size;
    // "Optimal" buffer size for I/O, as returned by "stat".
    // 0 if it can't be determined (e.g. on Windows)
    int io_buf_size;
};

// Same as "stat" except more portable.
// Caution: this returns a bool for success, false when errno == ENOENT or
// errno == ENOTDIR, and throws exception for error.
bool getFileStats(const Filename &f, FileStats &buf);
// That's lstat
bool getLinkStats(const Filename &f, FileStats &buf);
// This "stat" is more correct than MSVCRT's "stat" (which is broken
// by design somehow). So provide our own version that still takes a
// const char *. The "buf" argument provides an overload
// Bug 24801
// http://support.microsoft.com/kb/190315
bool getFileStats(const char *f, FileStats &buf);
bool getLinkStats(const char *f, FileStats &buf);

/**
 * Gets the file modification time in epoch seconds (last_modification
 * in FileStats); 0 if the file is not present.
 **/
::time_t get_last_write_time(const Filename &file);
/**
 * Sets the value of the last-modified time.  Returns true on success.
 * Otherwise, see errno for a system dependent error code.  Setting a file
 * read-only doesn't stop these functions from successfully setting its
 * modification time on linux but does on Windows.
 **/
bool set_last_write_time_now(const Filename &file); // sets write time to now
bool set_last_write_time(const Filename &file, ::time_t time);

bool dir_exists(const Filename &f);
bool file_exists(const Filename &f);

bool same_file(const Filename &p1, const Filename &p2);

void change_dir(const Filename &d);

// Redirection
bool redirect_all_output(const Filename &filename);
bool redirect_stdout(const Filename &filename);
bool redirect_stderr(const Filename &filename);

// is_subdirectory: return true if p1 is a sub-directory of p2.
bool is_subdirectory(const Filename& p1, const Filename& p2);

/**
 * Checks if \p p is a writable directory by trying to write to a
 * temporary file then cleaning up that file.
 * Also creates the directory if possible.
 **/
bool check_writeable(const Filename& p);

/**
 * Attempt to make file writable. Throws an XSystem exception on failure.
 **/
void set_file_writable(const Filename &file_name);
/**
 * Attempt to make file non-writable. Throws an XSystem
 * exception on failure.
 **/
void set_file_read_only(const Filename &file_name);
/**
 * Attempt to make the file writable, then remove it.
 * Throws an XSystem exception on failure.
 **/
void force_remove(const Filename &file_name);
/**
 * Recursively visits all files and subdirectories, setting them
 * writable and attempting to delete them.
 * Throws an XSystem exception on failure.
 **/
void force_remove_all(const Filename &dir_name);

/**
 * Creates file \p p if it doesn't exits, similar to unix \p touch
 * Throws an XSystem exception if it can't create.
 * Note: unlike "touch", the created file contains a newline, and the
 * file is not accessed if it already exists.
 **/
void do_touch(const Filename &);

/**
 * copy in \c from recursively to directory \p to (created if
 * necessary)
 * true - some files have been copied
 * false - no files have been copied
 **/
bool copy_files(const Filename &from, const Filename &to);

// Functions which work like their Boost counterparts in
// boost/filesystem/operations.hpp and filename{-fwd}.hpp
bool exists(const Filename &);
unsigned long long file_size(const Filename &);

void create_directory(const Filename &);
void create_directories(const Filename &);
bool remove(const Filename &);
unsigned remove_all(const Filename &);
void copy_file(const Filename &src, const Filename &dest);

/**
 * Rename a file on disk named \p from to \p to. This function will throw
 *   an exception if the operation fails.
 *
 * If the destination file exists, this function may either replace the
 *   destination file or throw an exception. On windows, this is guaranteed
 *   to throw an exception if the destination (\p to) exists. On Linux,
 *   this is platform and library specific based on cstdio rename
 *   (see http://www.cplusplus.com/reference/cstdio/rename).
 **/
void rename(const Filename &from, const Filename &to);
bool atomic_rename(const Filename &from, const Filename &to);

// Return a file f such that to/f is the same file as p
// Unlike the "path" version, this doesn't resolve symlinks unless it
// has to (to resolve <symlink>/..). If case_sensitive_windows is
// true, no conversion to lowercase will be done on Windows. This means
// that matching algorithm is case sensitive.
Filename make_relative(const Filename &p, const Filename &to, bool case_sensitive_windows);
Filename make_relative(const Filename &p, const Filename &to);
Filename get_windows_short_name(const Filename &);

// Return true if there is no other non-"super" user that can read the given
// file.  Throws XSystem on errors accessing file.
bool is_readable_only_by_owner(const Filename &);

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

class OrderedDirEntries {
public:
    explicit OrderedDirEntries(const Filename &);

    class iterator : public std::iterator<std::input_iterator_tag, const Filename> {
    public:
        iterator(const Filename &);
        iterator();
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
        class impl;
        refct_ptr<impl> impl_;
    };

    iterator begin() const;
    iterator end() const;

private:
    Filename dirname_;
};

inline DirEntries dir_entries(const Filename &f) {
    return DirEntries(f);
}
inline OrderedDirEntries ordered_dir_entries(const Filename &f) {
    return OrderedDirEntries(f);
}

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
 * foreach(i, recursive_dir_entries(dir)) iterates over all the files / directories
 * recursively contained in dir, in depth first preorder.
 **/
inline RecursiveDirEntries recursive_dir_entries(const Filename &dir) {
    return RecursiveDirEntries(dir);
}

class OrderedRecursiveDirEntries {
public:
    explicit OrderedRecursiveDirEntries(const Filename &dir):
        dirname_(dir) {
    }

    struct GetChildren;

    /**
     * An iterator on all the files/subdirectories of a given directory (recursively)
     * Directories are visited before their contents
     **/
    typedef recursive_iterator<OrderedDirEntries::iterator, GetChildren> iterator;

    struct GetChildren {
        pair<OrderedDirEntries::iterator, OrderedDirEntries::iterator> operator()(const Filename &dir);
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
 * foreach(i, recursive_dir_entries(dir)) iterates over all the files / directories
 * recursively contained in dir, in depth first preorder.
 **/
inline OrderedRecursiveDirEntries ordered_recursive_dir_entries(const Filename &dir) {
    return OrderedRecursiveDirEntries(dir);
}


CLOSE_NAMESPACE(FilenameNS);

void file_operations_unit_tests();

#endif // FILE_OPERATIONS_HPP
