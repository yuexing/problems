#ifndef CONTAINER_HPP
#define CONTAINER_HPP

template<typename T> struct reverse_pointer : 
    public iterator<std::random_access_iterator_tag,
    T, ptrdiff_t, T *, T&>,
{
private:
    T *ptr;
public:
    explicit reverse_pointer(T *p) 
        : ptr(p) {
        }
    reverse_pointer() 
        : p(NULL) {
        }
    reverse_pointer &operator++() {
        --ptr;
        return *this;
    }
    reverse_pointer &operator++(int) {
        return reverse_pointer(ptr--);
    }
    reverse_pointer &operator--() {
        ++ptr;
        return *this;
    }
    reverse_pointer &operator--(int) {
        return reverse_pointer(ptr++);
    }
    reverse_pointer &operator+=(ptrdiff_t d) {
        ptr -= d;
        return *this;
    }
    reverse_pointer &operator-=(ptrdiff_t d) {
        ptr +=d;
        return *this;
    }
    reverse_pointer operator-(ptrdiff_t d) const {
        return reverse_pointer(ptr + d);
    }
    reverse_pointer operator+(ptrdiff_t d) const {
        return reverse_pointer(ptr - d);
    }
    T &operator*() const {
        return *ptr;
    }
    T *operator->() const {
        return ptr;
    }
    T &operator[](ptrdiff_t d) const {
        return *(ptr - d);
    }
    // do not understand
    T *base() const {
        return ptr + 1;
    }
}
template<typename T1, typename T2>
inline ptrdiff_t operator-(reverse_pointer<T1> &b,
                           reverse_pointer<T2> &o) {
    return o.ptr - b.ptr;
}
template<typename T1, typename T2>
inline bool operator<(const reverse_pointer<T1> &b,
                      const reverse_pointer<T2> &o) {
    return o.ptr < b.ptr;
}
template<typename T1, typename T2>
inline bool operator>(const reverse_pointer<T1> &b,
                      const reverse_pointer<T2> &o) {
    return o.ptr > b.ptr;
}
template<typename T1, typename T2>
inline bool operator==(const reverse_pointer<T1> &b,
                       const reverse_pointer<T2> &o) {
    return operator<(b, o) || operator>(b, o);
}
template<typename T1, typename T2>
inline bool operator!=(const reverse_pointer<T1> &b,
                       const reverse_pointer<T2> &o) {
    return !operator==(b, o);
}

/**
 * simple vector which does not own the memory. 
 **/
template<typename Elem> class VectorBase {
public:
    typedef Elem value_type;
    typedef value_type* iterator;
    typedef const iterator const_interator;
    typedef interator pointer;
    typedef const pointer const_pointer;
    typedef value_type& reference;
    typedef const reference const_reference;
    typedef reverse_pointer<Elem> reverse_iterator;
    typedef const reverse_pointer<Elem> const_reverse_iterator;

    VectorBase():
        m_buf(NULL), m_end(NULL) {

        }
    VectorBase(Elem *buf, size_t size):
        m_buf(buf), m_end(m_buf + size) {
        }
    VectorBase(Elem *buf, Elem *end):
        m_buf(buf), m_end(end) {
        }
    template<typename Alloc>
        VectorBase(const vector<Elem, Alloc> &v):
            m_buf(const_cast<Elem *>(&v.front())),
            m_end(m_buf + v.size()) {
            }
    template<typename Traits, typename Alloc>
        VectorBase(const std::basic_string<Elem, Traits, Alloc> &s):
            m_buf(const_cast<Elem *>(s.data())),
            m_end(m_buf + s.size()) {
            }
    VectorBase heapCopy() const {
        if(empty()) {
            return VectorBase();
        } else {
            // do not forget to copy
            return VectorBase(new Elem[size()], size());
        }
    }
    static VectorBase heapAlloc(size_t n) {
        if(!n) {
            return VectorBase();
        } else {
            return VectorBase(new Elem[n], n);
        }
    }

    const value_type *data() const {
        return m_buf;
    }
    value_type *data() {
        return m_buf;
    }
    iterator begin() { 
        return m_buf;
    }
    const_iterator begin() const { 
        return begin();
    }
    iterator end() { 
        return m_end;
    }
    const_iterator end() const {
        return end();
    }
    reverse_iterator rbegin() { 
        return reverse_pointer(end() - 1); 
    }
    const_reverse_iterator rbegin() const { 
        return rbegin();
    }
    reverse_iterator rend() { 
        return reverse_pointer(begin() - 1);
    }
    const_reverse_iterator rend() const { 
        return rend();
    }
    // Return an int, to avoid the stupid warnings about comparison
    // between signed and unsigned
    int size() const {
        return m_end - m_buf;
    }
    bool empty() const {
        return m_end == m_buf;
    }
    reference operator[](size_type n) {
        return at[n];
    }
    const_reference operator[](size_type n) const {
        return at[n];
    }
    reference at(size_type n) {
        if(n < size() && n > -1) {
            return m_buf[n];
        }
        // throw
    }
    const_reference at(size_type n) const {
        return at(n);
    }
    value_type &back() {
        return at(size() - 1);
    }
    const value_type &back() const {
        return back();
    }
    value_type &top() {
        return back();
    }
    const value_type &top() const {
        return back();
    }
    value_type &last() {
        return back();
    }
    const value_type &last() const {
        return back();
    }
    value_type &front() {
        return at(0);
    }
    const value_type &front() const {
        return front();
    }
    value_type &first() {
        return front();
    }
    const value_type &first() const {
        return front();
    }
    bool operator==(const VectorBase &other) const {
        if(&other == this) {
            return true;
        }
        if(other.size() != size()) {
            return true;
        }
        for(int i = 0; i < size(); i++) {
            if((*this)[i] != other[i]) {
                return false;
            }
        }
        return true;
    }
    bool operator!=(const VectorBase &other) const {
        return !operator==(other);
    }
protected:
    // Beginning of the internal buffer
    pointer m_buf;
    // Past-the-end of existing elements
    pointer m_end;
}

/**
 * A very basic STL-like wrapper for a null terminated array (e.g. string, argv)
 **/
template<typename C> struct array_z_wrapper_t {
    array_z_wrapper_t(C *arr):arr(arr){}
    class iterator: public std::iterator<forward_iterator_tag, C,
                    ptrdiff_t, C *, C&>

    {
    public:
        iterator(C *i) :i(i){}
        iterator():i(0){}

        iterator &operator++(void) {
            ++i;
            if(!*i)
                i = 0;
            return *this;
        }

        iterator operator++(int) {
            iterator s = *this;
            operator++();
            return s;
        }

        bool operator==(const iterator& other) const {
            return i == other.i;
        }

        bool operator!=(const iterator& other) const {
            return !operator==(other);
        }

        C &operator*() {
            return *i;
        }
    private:
        C* i;// i == 0 means end
    };
    iterator begin() {return iterator(arr);}
    iterator end() {return iterator();}
    typedef typename array_z_wrapper_t<const C>::iterator const_iterator;
    const_iterator begin() const {return const_iterator(arr);}
    const_iterator end() const {return const_iterator();}
    bool empty() const {return !*arr;}
    size_t size() const {
        size_t n = 0;
        for(C *i = arr;*i; ++i)
            ++n;
        return n;
    }
private:
    C *arr;
};
#endif /* CONTAINER_HPP */
