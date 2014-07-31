// Design a Message store system  (in-memory storage) [seq_id, len, data] 
// chunk

// Design monitoring system, circular array, storage, aggregation

// Design tinyURL

// Design a key / value system, key-value store

// Design a feeds system, write and query

// webserver, and the event-counter is to be called for event counting.
class EventCounter{
public:
    void Increment() ;
    //
    int getLastSecondCount();
    //
    int getLastDayCount();
};
