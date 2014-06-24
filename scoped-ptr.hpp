#ifndef SCOPED_PTR_HPP
#define SCOPED_PTR_HPP
template<typename T>
class scoped_ptr_t
{
public:
    scoped_ptr_t() : 
        ptr(0) {
        }

    scoped_ptr_t(T *ptr) :
        ptr(ptr) {
        }

    void reset(T* p = NULL) {
        delete ptr;
        ptr = p;
    }

    ~scoped_ptr_t() {
        reset();
    }

private:
    // do not allow
    scoped_ptr_t(const scoped_ptr_t &o);
    scoped_ptr_t& operator=(const scoped_ptr_t &o);
    T *ptr;
};

// scoped_array_t
#endif /* SCOPED_PTR_HPP */
