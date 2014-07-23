#ifndef ITERATORS_HPP
#define ITERATORS_HPP

/**
 * Practice writing iterators compitable with STL in cpp, the iterator_traits
 * will have the following info:
 * NB: 
 * iterator category:
 *  std::input_iterator_tag,
 *  std::output_iterator_tag,
 *  std::forward_iterator_tag,
 *  std::bidirectional_iterator_tag
 *  std::random_access_iterator_tag
 * iterator_category
 * value_type
 * pointer
 * reference
 * difference_type
 */

template <class T, class A = std::allocator<T> >
class X {
public:
    typedef A allocator_type;
    typedef typename A::value_type value_type; 
    typedef typename A::reference reference;
    typedef typename A::const_reference const_reference;
    typedef typename A::difference_type difference_type;
    typedef typename A::size_type size_type;

    class iterator { 
    public:
        typedef typename A::difference_type difference_type;
        typedef typename A::value_type value_type;
        typedef typename A::reference reference;
        typedef typename A::pointer pointer;
        typedef std::random_access_iterator_tag iterator_category; //or another tag

        iterator();
        iterator(const iterator&);
        ~iterator();

        iterator& operator=(const iterator&);
        bool operator==(const iterator&) const;
        bool operator!=(const iterator&) const;
        bool operator<(const iterator&) const; //optional
        bool operator>(const iterator&) const; //optional
        bool operator<=(const iterator&) const; //optional
        bool operator>=(const iterator&) const; //optional

        iterator& operator++();
        iterator operator++(int); //optional
        iterator& operator--(); //optional
        iterator operator--(int); //optional
        iterator& operator+=(size_type); //optional
        iterator operator+(size_type) const; //optional
        friend iterator operator+(size_type, const iterator&); //optional
        iterator& operator-=(size_type); //optional            
        iterator operator-(size_type) const; //optional
        difference_type operator-(iterator) const; //optional

        reference operator*() const;
        pointer operator->() const;
        reference operator[](size_type) const; //optional
    };
    class const_iterator {
    public:
        typedef typename A::difference_type difference_type;
        typedef typename A::value_type value_type;
        typedef typename A::reference const_reference;
        typedef typename A::pointer const_pointer;
        typedef std::random_access_iterator_tag iterator_category; //or another tag

        const_iterator ();
        const_iterator (const const_iterator&);
        const_iterator (const iterator&);
        ~const_iterator();

        const_iterator& operator=(const const_iterator&);
        bool operator==(const const_iterator&) const;
        bool operator!=(const const_iterator&) const;
        bool operator<(const const_iterator&) const; //optional
        bool operator>(const const_iterator&) const; //optional
        bool operator<=(const const_iterator&) const; //optional
        bool operator>=(const const_iterator&) const; //optional

        const_iterator& operator++();
        const_iterator operator++(int); //optional
        const_iterator& operator--(); //optional
        const_iterator operator--(int); //optional
        const_iterator& operator+=(size_type); //optional
        const_iterator operator+(size_type) const; //optional
        friend const_iterator operator+(size_type, const const_iterator&); //optional
        const_iterator& operator-=(size_type); //optional            
        const_iterator operator-(size_type) const; //optional
        difference_type operator-(const_iterator) const; //optional

        const_reference operator*() const;
        const_pointer operator->() const;
        const_reference operator[](size_type) const; //optional
    };

    typedef std::reverse_iterator<iterator> reverse_iterator; //optional
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator; //optional

    X();
    X(const X&);
    ~X();

    X& operator=(const X&);
    bool operator==(const X&) const;
    bool operator!=(const X&) const;
    bool operator<(const X&) const; //optional
    bool operator>(const X&) const; //optional
    bool operator<=(const X&) const; //optional
    bool operator>=(const X&) const; //optional

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;
    reverse_iterator rbegin(); //optional
    const_reverse_iterator rbegin() const; //optional
    const_reverse_iterator crbegin() const; //optional
    reverse_iterator rend(); //optional
    const_reverse_iterator rend() const; //optional
    const_reverse_iterator crend() const; //optional

    reference front(); //optional
    const_reference front() const; //optional
    reference back(); //optional
    const_reference back() const; //optional
    template<class ...Args>
        void emplace_front(Args...); //optional
    template<class ...Args>
        void emplace_back(Args...); //optional
    void push_front(const T&); //optional
    void push_front(T&&); //optional
    void push_back(const T&); //optional
    void push_back(T&&); //optional
    void pop_front(); //optional
    void pop_back(); //optional
    reference operator[](size_type); //optional
    const_reference operator[](size_type) const; //optional
    reference at(size_type); //optional
    const_reference at(size_type) const; //optional

    template<class ...Args>
        iterator emplace(const_iterator, Args...); //optional
    iterator insert(const_iterator, const T&); //optional
    iterator insert(const_iterator, T&&); //optional
    iterator insert(const_iterator, size_type, T&); //optional
    template<class iter>
        iterator insert(const_iterator, iter, iter); //optional
    iterator insert(const_iterator, std::initializer_list<T>); //optional
    iterator erase(const_iterator); //optional
    iterator erase(const_iterator, const_iterator); //optional
    void clear(); //optional
    template<class iter>
        void assign(iter, iter); //optional
    void assign(std::initializer_list<T>); //optional
    void assign(size_type, const T&); //optional

    void swap(const X&);
    size_type size();
    size_type max_size();
    bool empty();

    A get_allocator(); //optional
};
template <class T, class A = std::allocator<T> >
void swap(X<T,A>&, X<T,A>&); //optional

class verify;
class tester {
    friend verify;
    static int livecount;
    const tester* self;
public:
    tester() :self(this) {++livecount}
    tester(const tester&) :self(this) {++livecount}
    ~tester() {assert(self==this);--livecount;}
    tester& operator=(const tester& b) {
        assert(self==this && b.self == &b);
        return *this;
    }
    void cfunction() const {assert(self==this);}
    void mfunction() {assert(self==this);}
};
struct verify() {
    ~verify() {assert(livecount==0);}
}varifier;

//// some interesting iterators to implement
// through operator++, we are only interested in the elements which can pass
// the filter_fun.
template<typename it, typename filter_fun> 
class filter_iterator_t : public iterator
                          <bidirectional_iterator_tag,
                          typename iterator_traits<it>::value_type,
                          typename iterator_traits<it>::difference_type,
                          typename iterator_traits<it>::pointer,
                          typename iterator_traits<it>::reference >
{

private:
    it b, e; // begin, end
    filter_fun f; // the function
};


// NB: the class is of the it::iterator_category, thus needs more operator
// functions.
// BUt the point is that while opertor*, operator[], we return the f(element).
template<typename it, typename map_fun> 
class mapped_iterator : public iterator
                        <typename iterator_traits<it>::iterator_category,
                        typename map_fun::result_type,
                        typename iterator_traits<it>::difference_type,
                        typename map_fun::result_type *,
                        typename map_fun::result_type>
{

private:
    it b, e; // begin, end
    map_fun f; // the function
};

// Also, a iterator of several iterators.
template<typename it> 
class union_iterator : public iterator
                        <typename iterator_traits<it>::iterator_category,
                        typename map_fun::result_type,
                        typename iterator_traits<it>::difference_type,
                        typename map_fun::result_type *,
                        typename map_fun::result_type>
{

}

// a iterator for recursive operation, eg. traversing a tree
// a non-recurisive version of :
// pre-order:
/* while(root || !stack.empty()) {
        if(root) {
            visit(root);
            root = root->left;
            if(root->right) {
            stack.push(root->right);
            }
        } else {
            root = stack.top();
            stack.pop();
        }
 } */
// post-order
template<typename it, typename get_children_t> 
class recursive_iterator : public iterator
                           <std::forward_iterator_tag,
                           typename iterator_traits<it>::value_type,
                           typename iterator_traits<it>::difference_type,
                           typename iterator_traits<it>::pointer,
                           typename iterator_traits<it>::reference>
{

}
#endif /* ITERATORS_HPP */

// TODO:
// Program an iterator for a Linked List which may include nodes which are nested within other nodes. i.e. (1)->(2)->(3(4))->((5)(6). Iterator returns 1->2->3->4->5->6
