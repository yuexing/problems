#include <queue>
using namespace std;

extern int getpid();
extern void deschedule(bool);
extern void make_runnable(int);

extern int xchg(int*, int);
// spin-lock
struct spin_lock_t
{
    void lock() {
        while(xchg(&avail, 0) == 0) /*yield to the owner.*/;
    }

    // locked, so no need for the xchg
    void unlock() {
        avail = 1;
    }

    spin_lock_t(): avail(1) {
    }
private:
    int avail;
};

struct wait_node_t
{
    int pid;
    bool to_wait;

    wait_node_t(int pid): pid(pid), to_wait(true) {
    }
};

// FIFO queue lock
struct lock_t
{
    void lock() {
        alock.lock();
        if(!avail) {
            wait_node_t n(getpid());
            q.push(n);
            alock.unlock(); 

            deschedule(n.to_wait);
        } else {
            alock.unlock(); 
        }
    }

    void unlock() {
        alock.lock();
        // assert(avail == 0);
        if(q.empty()) {
            avail = 1;
            alock.unlock();
        } else {
            wait_node_t n = q.front();
            q.pop();
            alock.unlock();

            n.to_wait = false;
            make_runnable(n.pid); 
        }
    }

private:
    spin_lock_t alock;

    int avail; 
    queue<wait_node_t> q; 
};

// condvar
struct condvar_t 
{
    void wait(lock_t &lock)
    {
        qlock.lock();
        wait_node_t n(getpid());
        q.push(n);
        lock.unlock(); // suppose the seq does not matter now.
        qlock.unlock(); 

        deschedule(n.to_wait);
        lock.lock();
    }

    void signal()
    {
        qlock.lock();
        if(!q.empty()) {
            wait_node_t n = q.front();
            q.pop();
            qlock.unlock();

            n.to_wait = false;
            make_runnable(n.pid); 
        } else {
            qlock.unlock();
        }
    }

    void broadcast()
    {
        qlock.lock();
        while(!q.empty()) {
            wait_node_t n = q.front();
            q.pop();
            n.to_wait = false;
            make_runnable(n.pid);
        }
        qlock.unlock();
    }

private:
    queue<wait_node_t> q;
    lock_t qlock;
};

// semaphore
struct sem_t
{
    sem_t(int resources): resources(resources){
    }

    void wait() {
        rlock.lock();
        while(resources <= 0) {
            rcond.wait(rlock);
        }
        --resources;
        rlock.unlock();
    }

    void signal() {
        rlock.lock();
        ++resources;
        if(resources - 1 <= 0) {
            rcond.signal();
        }
        rlock.unlock();
    }

private:
    int resources;
    lock_t rlock;
    condvar_t rcond;
};

namespace producer_customer {
    sem_t empty(10), full(0);
    lock_t glock; /* to operate on a queue */

    struct producer {
        void run() {
            empty.wait();
            full.signal();
        }
    };

    struct consumer {
        void run() {
            full.wait();
            empty.signal();
        }
    };
};

// read-write lock
// a typical reader-biased lock
struct rw_lock_t
{
    enum rw_lock_type_t {
        RW_LOCK_READ,
        RW_LOCK_WRITE
    };

    void lock(rw_lock_type_t ltype)
    {
        if(ltype == RW_LOCK_READ) {
            cnt_lock.lock();
            while(writer_pid > 0 || waiting_writers > 0) {
                ++waiting_readers;
                rcond.wait(cnt_lock);
                --waiting_readers;
            }
            ++readers;
            cnt_lock.unlock();
        } else if(ltype == RW_LOCK_WRITE) {
            cnt_lock.lock();
            while(readers > 0 || writer_pid > 0) {
                ++waiting_writers;
                wcond.wait(cnt_lock);
                --waiting_writers;
            }
            writer_pid = getpid();
            cnt_lock.unlock();
        } else {
            // failure
        }
    }

    // TODO
    void upgrade()
    {

    }

    // TODO
    void downgrade()
    {

    }

    void unlock()
    {
        cnt_lock.lock();
        bool enable_readers = false;
        bool enable_writers = false;
        if(writer_pid > 0) {
            writer_pid = -1;
            enable_readers = waiting_readers > 0;
        } else {
            if(--readers == 0)
                enable_writers = waiting_writers > 0;
        }     
        if(enable_readers) {
            rcond.broadcast();
        } else if(enable_writers) {
            wcond.signal();
        }
        cnt_lock.unlock();
    }

private:
    lock_t cnt_lock;
    condvar_t rcond, wcond;
    int waiting_writers, waiting_readers, readers, writer_pid;
};

// TODO: manipulate the list lock-free in multi-thread env.
extern void* cxchg(void *from, void *to, void* expect);
namespace {
struct list_node_t
{
    int v;
    list_node_t *next;

    list_node_t(int v): v(v), next(NULL) {}
};

struct list_t
{
    list_t(): head(-1), tail(&head) {}

    void push(int v)
    {
        list_node_t *n = new list_node_t(v);
        while(cxchg(tail->next, NULL, n) == NULL);
        tail = n; // this is atomic 
    }

    void insert_after(list_node_t *t, list_node_t* v)
    {
        do {
            v->next = t->next; 
        } while(cxchg(v, t->next, v->next) == v->next);
    }

    void delete_after(list_node_t *t)
    {
        list_node_t* v;
        do {
            v = t->next;
        } while(cxchg(t->next, v, v->next) == v);
        // NB: at this state, both v and t are pointing to the same v->next,
        // which is very unsafe!
        // So, the difference btw insert and delete is that the node to be deleted 
        // has already been shared.
        delete v;
    }

    // This version of push_front/pop_front can result to the ABA problem.
    // global = new...;
    //                            local2 = global;
    // local1 = global;          
    // free(local1);         
    // global = new...;          
    //                           if(local2 == global)
    // btw the 2 reads for the second thread, the global has already changed!
    // When the global is the head, and the test is cxchg(head, local2, local2->next),
    // the list will result in pointing to garbage.
    void pop()
    {
       list_node_t *n;
       do {
           n = head.next;
       } while(cxchg(head.next, n, n->next));
    }

private:
    list_node_t head, *tail;
};

// RCU:
// rcu_read_lock, rcu_read_unlock: non-preemptive
// rcu_fetch, rcu_assign
// rcu_synchronize: wait for all context-switch have finished.
}

// safe delete a folder in multi-thread env
// lock on the edge? 
// lock-handoff: lock(prev) -> lock(root), lock(next), unlock(prev). But the chilren of the prev suffers multi-thread!
// read-write lock on each node? Yes!
