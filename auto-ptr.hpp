#ifndef AUTO_PTR_HPP
#define AUTO_PTR_HPP
// auto_ptr_t has different semantics from xxx_ptr_t, as it grabs the
// ownership to a ptr while copy and assign, by changing the source. Thus it
// is deprecated.
template<typename T>
class auto_ptr_t {
public:
    auto_ptr_t()
        :ptr(0) {
        }

    auto_ptr_t(T *ptr)
        :ptr(ptr) {
        }

    auto_ptr_t(auto_tpr_t &o)
        :ptr(o.ptr) {
            o.ptr = NULL;
        }

    auto_ptr_t& operator=(auto_ptr_t &o) 
    {
        reset(o.ptr);
        return *this;
    }

    ~auto_ptr_t() {
        reset();
    }

    void reset(T* p = NULL) {
        delete ptr;
        ptr = p;
    }
private:
    T *ptr;
};

// A better solution may be an owner_t, which always clones the source.
// Actually: auto_ptr_t => uniq_ptr_t
// or                   => owner_ptr_t
template<typename T>
class owner_t {
public:
    owner_t()
        :ptr() {
        }

    owner_t(T *p)
        :ptr(p) {
        }

    owner_t(const owner_t &o) 
        :ptr(dup(o.ptr)){
    }

    owner_t& operator=(owner_t &o) 
    {
        reset(dup(o.ptr));
        return *this;
    }

    ~owner_t(){
        reset();
    }

    void reset(T *p = NULL) {
        delete ptr;
        ptr = p;
    }
private:
    T *dup(T *p) {
        return new T(*p);
    }
    T *ptr;
};
#endif /* AUTO_PTR_HPP */
