#ifndef LINE_STREAM_HPP
#define LINE_STREAM_HPP
#include <istream>
/*
 * Implement a line iterator. Given an istream, it iterates all the lines in
 * the stream.
 */
struct line_iterator
{
    line_iterator(istream &is);
    // fill up cur_line
    void operator++();
    // return cur_line
    string &opertor*();
    // compare to the end(is == NULL)
    bool operator==();
private:
    // NB: iterator will be copied all the time, thus it's the pointer here.
    istream *is;
    // line
    string cur_line;
    // buf
    static const int buf_size = 0x100;
    char buf[buf_size];
    char *buf_start, *buf_end;
};

/*
 * Always output by lines.
 */
struct line_ostream
    : public ostream
{
    line_ostream_buf
        : public streambuf {
            // Output
            override traits_type::int_type overflow(traits_type::int_type ch);
            override streamsize xsputn(const traits_type::char_type* s, streamsize n);
        }
};
#endif /* LINE_STREAM_HPP */
