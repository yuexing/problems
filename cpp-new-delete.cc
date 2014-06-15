#include <new>
using namespace std;

template<class T>
class NewHandlerSupport
{
public:
    static new_handler set_new_handler(new_handler p)
    {
        new_handler old = current_handler;
        current_handler = p;
        return old;
    }

    static void *operator new(size_t size) {
        new_handler g = std::set_new_handler(current_handler);
        try {
            ::operator new(size); // if this fail, it will call the current
        } catch (std::bad_alloc&) {
            std::set_new_handler(g); // if the current fails, then g again
        }
        std::set_new_handler(g);
    }
private:
    static new_handler current_handler;
};

template<typename T>
new_handler NewHandlerSupport<T>::current_handler = NULL;

template<class T>
class NoThrowSupport
{
public:
    void *operator new(size_t size) {
        return ::operator new(size, std::nothrow);
    }
};

// Mixin-style inheritance
class X : public NewHandlerSupport<X>
{
};

class Y : public NoThrowSupport<Y>
{
};
