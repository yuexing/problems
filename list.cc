// some list problem
struct list_node_t
{
    int v;
    list_node_t *next;
};

// return the 1/3, 2/3 node from a list
void printThirds(list_node_t *head)
{

}

// a circular forward list
// whose add, remove is O(1)
struct circle_list_t
{
    void add(int v)
    {
        list_node_t *n = new list_node_t();
        if(tail == 0) {
            head = tail = n;
        } else {
            tail->next = n;
            tail = n;
        }
        tail->next = head;
    }

    bool empty()
    {
        return head == 0;
    }

    int pop_front()
    {
        // assert(!empty());
        list_node_t *n = head;
        int ret = n-> v;
        head = head->next;    
        tail->next = head;
        delete n;
        return ret;
    }

    int pop_back()
    {
        list_node_t *n = tail;
        int ret = n-> v;
        tail->v = head->v;
        (void)pop_front();
        return ret;
    }
private:
    list_node_t *head, *tail;
};

// Find the maximum contiguous sum in a circular linked list.
//
// run the typical maximum subarray algorithm with comparing sum_so_far 
// with the current element. To take into account the circular part, 
// continue run the algorithm until 1) it stops; 2) reach the beginning
// of the current subarray.

