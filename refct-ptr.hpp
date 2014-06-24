#ifndef REFCT_PTR_HPP
#define REFCT_PTR_HPP

template<typename T>
void checked_delete(T *ptr)
{
    typedef char type_must_be_complete[sizeof(T)? 1: -1];
    (void) sizeof(type_must_be_complete);
    delete ptr;
}

template<typename T>
class refct_ptr_t
{
public:
    refct_ptr_t() : 
        ptr(0),
        cnt(0) {
            cnt = new int(0);
        }

    refct_ptr_t(T *ptr) :
        ptr(ptr),
        cnt(ptr? new int(1) : 0){
        }

    refct_ptr_t(const refct_ptr_t &o) 
    {
        set(o);
    }

    refct_ptr_t& operator=(const refct_ptr_t &o) {
        if(ptr == o.ptr) {
            return *this;
        }
        reset();
        set(o);
        return *this;
    }

    void set(const refct_ptr_t &o)
    {
        cnt = o.cnt;
        ptr = o.ptr;
        if(ptr)
            ++*cnt;
    }

    void reset(T* p = NULL) {
        if(!(--*cnt)) {
            checked_delete(ptr);
            delete cnt;
        }
        ptr = p;
        if(ptr) {
            cnt = new int(1);
        } else {
            cnt = 0;
        }
    }

    ~refct_ptr_t() {
        reset();
    }
private:
    T *ptr;
    int *cnt;
};
// refcnt_array_t
#endif /* REFCT_PTR_HPP */
