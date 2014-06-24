/**
 * Least Recently Used cache:
 * get(key)
 * set(key, value)
 */

class LRUCache {
public:
    LRUCache(int capacity)
        :capacity(capacity) {}

    // return -1 if not exist
    int get(int key) 
    {
        if(contains(dict, key)) {
            return *dict[key];
        } else {
            return -1;
        }
    }

    void set(int key, int value) 
    {
        if(contains(dict, key)) {
            *dict[key] = value;
            move_to_front(key);
        } else {
            mtf.push_front(value);
            dict[key] = mtf.front();
        }
        evict_older();
    }

    void move_to_front(int key) 
    {
        mtf.splice(mtf.front(), mtf, dict[key]);
        dict[key] = mtf.front();
    }

    // is it worth evicting half-of-the-size?
    void evict_older()
    {
        while(mtf.size() > capacity) {
            mtf.erase(mtf.back());
        }
    }

private:
    int capacity;
    list<int> mtf;
    typedef list<int>::iterator mtf_it;
    map<int, move_to_front_it> dict;
};

// a more general LRU cache can be made up of [key, entry], in which the entry
// contains a timestamp. While evicting (half-of-the-size), build a heap in a
// vector(of the size-to-delete) using std::push_heap as well as filter down.
