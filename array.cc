#include <iostream>
using namespace std;

// wrapper for a reverse iterator

template<typename T, typename iterator_type_t>
class reverse_iterator_t
{
public:
    reverse_iterator_t(iterator_type_t i) : i(i) {
    }
    reverse_iterator_t(const reverse_iterator_t& o): i(o.i) {
    }
    reverse_iterator_t& operator=(const reverse_iterator_t& o) {
        i = o.i;
        return *this;
    }

    // operators
    reverse_iterator_t& operator++() {
        --i;
        return *this;
    }
    reverse_iterator_t operator++(int) {
        return reverse_iterator_t (i--);
    }
    reverse_iterator_t operator--() {
        ++i;
        return *this;
    }
    reverse_iterator_t operator--(int) {
        return reverse_iterator_t(i++);
    }

    reverse_iterator_t operator+(int x)
    {
        return reverse_iterator_t(i - x);
    }
    reverse_iterator_t operator-(int x)
    {
        return reverse_iterator_t(i + x);
    }

    bool operator==(const reverse_iterator_t& o) {
        return i == o.i;
    } 
    bool operator!=(const reverse_iterator_t& o) {
        return !operator==(o);
    }
    T& operator*() {
        return *i;
    }
private:
    iterator_type_t i;
};

// wrapper for the original array
template<typename T>
class Array {
public:
    Array(): arr(0), size(0) {
    }
    Array(T* arr, int size): arr(arr), size(size) {
    }

    // iterator is just simple
    typedef T value_type;
    typedef value_type* iterator_type;
    typedef const value_type* const_iterator_type;
    typedef reverse_iterator_t<T, iterator_type> reverse_iterator_type;
    typedef reverse_iterator_t<const T, const_iterator_type> const_reverse_iterator_type;

    iterator_type begin() {
        return arr;
    }
    iterator_type end() {
        return arr + size;
    }
    reverse_iterator_type rbegin() {
        return reverse_iterator_type(end() - 1);
    }
    reverse_iterator_type rend() {
        return reverse_iterator_type(begin() - 1);
    }
    const_iterator_type begin() const {
        return arr;
    }
    const_iterator_type end() const {
        return arr + size;
    }
    const_reverse_iterator_type rbegin() const {
        return const_reverse_iterator_type(end() - 1);
    }
    const_reverse_iterator_type rend() const {
        return const_reverse_iterator_type(begin() - 1);
    }

    // implicit cast
    operator const T *() const {
        return arr;
    }
    operator const T* () {
        return arr;
    }
    operator T* () {
        return arr;
    }

    bool empty() {
        return size == 0;
    }
private:
    // disable copy and assign
    Array(const Array&);
    Array& operator=(const Array&);
    // data members
    T* arr;
    int size;
};

void test_const(const Array<int>& a)
{
    for(Array<int>::const_reverse_iterator_type it = a.rbegin();
        it != a.rend();
        ++it) {
        cout << *it << ", ";
    }
    cout << endl;
}

int main()
{
    int arr[10];
    Array<int> a(arr, 10);
    int i = 0;
    for(Array<int>::iterator_type it = a.begin();
        it != a.end();
        ++it) {
        *it = ++i;
    }

    for(Array<int>::iterator_type it = a.begin();
        it != a.end();
        ++it) {
        cout << *it << ", ";
    }

    cout << endl;

    for(Array<int>::reverse_iterator_type it = a.rbegin();
        it != a.rend();
        ++it) {
        cout << *it << ", ";
    }

    cout << endl;

    test_const(a);
}
