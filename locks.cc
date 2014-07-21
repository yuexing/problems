extern "C" int xchg(int);
// spin-lock
struct spin_lock_t
{
    void lock() {
        while(!xchg(&avail, 0)) /*yield to the owner.*/;
    }

    // locked, so no need for the xchg
    void unlock() {
        avail = 1;
    }

    spin_lock_t(): lock(1) {
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
            q.push_back(n);
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
            wait_node_t n = q.pop();
            alock.unlock();

            n.to_wait = false;
            schedule(n.pid); 
        }
    }

private:
    spin_lock_t alock;

    int avail; 
    queue<int> q; 
};

// condvar
struct condvar_t 
{
    void wait(lock_t &lock)
    {
        qlock.lock();
        wait_node_t n(getpid());
        q.push_back(n);
        lock.unlock(); // suppose the seq does not matter now.
        qlock.unlock(); 

        deschedule(n.to_wait);
        lock.lock();
    }

    void signal()
    {
        qlock.lock();
        if(!q.empty()) {
            wait_node_t n = q.pop();
            qlock.unlock();

            n.to_wait = false;
            schedule(n.pid); 
        } else {
            qlock.unlock();
        }
    }

    void broadcast()
    {
        qlock.lock();
        while(!q.empty()) {
            wait_node_t n = q.pop();
            n.to_wait = false;
            schedule(n.pid);
        }
        qlock.unlock();
    }

private:
    queue<int> q;
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
        --resource;
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
            empty.singal();
        }
    }
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
};
