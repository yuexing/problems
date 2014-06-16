#include <iostream>
using namespace std;

// the stream library does:
// 1) formatting
// - ios
// - include
//   - width
//   - filled char
//   - alignment
//   - base
//   - floating point format (fixed, scientific, or general)
//   - sign +?,  trailing zeros
// 2) bufferring
// - 3 functions
//  - streambuf::overflow(int ch);
//  - streambuf::underflow();
//  - streambuf::sync()

struct set_format_t {
    typedef void(*m_t)(ostream &, const void *arg);
    set_format_t(m_t m, const void *arg) : m(m), arg(arg) {
    }

    friend ostream &operator<<(ostream &os, const set_format_t &s);

private:
    m_t m;
    const void *arg;
};

ostream &operator<<(ostream &os, const set_format_t &s)
{
    s.m(os, s.arg);
    return os;
}

// TODO: implement this function with info in
// http://www.horstmann.com/cpp/iostreams.html
void do_setformat(ostream &os, const void *arg)
{
    os << (const char*)arg << endl;
}

/* buffering
class DebugStream : public ostream
{
public:
    DebugStream() : ostream(new DebugStreamBuffer()), ios(0) {}
    ~DebugStream() { delete rdbuf(); }
    void open(const char fname[] = 0) { _buf.open(fname); }
    void close() { _buf.close(); }
};

class DebugStreamBuffer : public filebuf
{
public:
    DebugStreamBuffer() { filebuf::open("NUL", ios::out); }
    void open(const char fname[]);
    void close() { _win.close(); filebuf::close();} 
    virtual int sync();
private:
    DebugStreamWindow _win;
};

void DebugStreamBuffer::open(const char fname[])
{  close();
    filebuf::open(fname ? fname : "NUL", 
                  ios::out | ios::app | ios::trunc);
    _win.open(fname);
} 

int DebugStreamBuffer::sync()
{  int count = out_waiting();
    _win.append(pbase(), count);
    return filebuf::sync(); 
}*/

int main()
{
    set_format_t s(do_setformat, (const void*)"%+06d");
    cout << s ;
    return 0;
}
