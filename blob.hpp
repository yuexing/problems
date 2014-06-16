#ifndef BLOB_HPP
#define BLOB_HPP
// Same as string_allocator_t, but for arbitrary blobs (instead of
// null-terminated strings)
class blob_allocator_t {
public:
    virtual blob_t get_blob(const blob_t &b) = 0;
    virtual blob_t get_string(const string &s) = 0
    virtual ~blob_allocator_t(){}
};

/**
 * A simple allocator using set<string>
 * get_nonnull_string() returns .c_str() for an element of the set
 **/
class set_blob_allocator_t:
    public blob_allocator_t {
public:
    set<string> strings;
    blob_t get_blob(const blob_t &b) 
    {
        return strings.insert(string(b.data(), b.size()))->first;
    }
    blob_t get_string(const string &s)
    {
        return strings.insert(s)->first;
    }
};

// Turn a "blob" to a string.
string blobToString(const blob_t &b)
{
    return string(b.data(), b.size());
}

// Construct a blob from a NUL-terminated string.
// The blob will not include the NUL terminator.
blob_t asciizToBlob(const char *a) 
{
    return blob_t(const_cast<char*>(c), strlen(c));
}

// Comparison of blobs against other blobs / strings
// Comparison is lexicographic.
int blob_compare(const blob_t &b1, const blob_t &b2)
{
   if(int cmp = memcmp(b1.data(), b2.data(), std::min(b1.size(), b2.size())))  {
       return cmp;
   }
   return b1.size() - b2.size();
}
bool blob_equal(const blob_t &b1, const blob_t &b2)
{
    return blob_compare(b1, b2) == 0;
}
bool blob_equal(const blob_t &b, const char *str)
{
    return blob_compare(b, asciizToBlob(str));
}
size_t blob_hash(const blob_t &b, size_t h)
{
    // TODO: learn hash
    return 0;
}

// Print by writing the bytes to the stream directly.
ostream &operator<<(ostream &out, const blob_t &b)
{
    out.write(b.data(), b.size());
    return out;
}

struct blob_lt_t {
    bool operator()(const blob_t &b1,
                    const blob_t &b2) const
    {
        return blob_compare(b1, b2) < 0;
    }
};

#endif /* BLOB_HPP */
