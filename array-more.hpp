/**
 *
 */
// given a size generate an array
// offer implicit casts, [], begin, end
template <class T> class PlainArray;

// a grow array can be copied/assigned
// also grow the array to the asked index by filling the elements in btw with
// default constructor
template <class T> class GrowArray
{
    struct iterator {
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef int difference_type;
        typedef T* pointer;
        typedef T& reference;
        typedef int ptrdiff_t;

        iterator(GrowArray<T>& arr, int where) :
            arr(arr),
            where(where) {
            }
    };
};
// compared to the GrowArray, it gives the interface for pop/push.
template <class T> class ArrayStack;
