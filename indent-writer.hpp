#ifndef INDENT_WRITER_HPP
#define INDENT_WRITER_HPP
// Implement this.
/**
 * Wrapper for a stream that can print indentation.
 * Indentation is printed before the first character and before the
 * first character after any newline, unless "indent_empty_lines" is
 * false, in which case it's only printed that first character is not
 * itself a newline.
 *
 * "indent" can be called at the beginning or right after a newline.
 * "dedent" can only be called right after a newline.
 **/
class IndentWriter : public ostream {
  public:
    IndentWriter(ostream &out, int indentWidth = 2, bool indent_empty_lines = true);

    ~IndentWriter();

    // manipulate depth
    virtual void up();
    virtual void down();

    // alternate names
    void indent();
    void dedent();

    virtual void finish();

    class IndentWriterBuf:
        public std::streambuf {
    public:
        IndentWriterBuf(
            ostream &out,
            int indentWidth,
            bool indent_empty_lines);
        ostream &out;                 // output to here
        int indentWidth;              // Number of spaces for a single
                                      // level of indentation
        int depth;                    // depth of indentation
        // This indicates that we should indent before printing the
        // next character.
        bool has_pending_indentation;
        bool indent_empty_lines;

        void up();
        void down();

        // indentation and formatting support
        void printIndentation();
        void finish();

    protected:
        streamsize xsputn(const char_type *str, streamsize count);
        int overflow(int ch);
    } buf;
};

// indent on entry, dedent on exit
class IndentWriterIndenter {
  NO_OBJECT_COPIES(IndentWriterIndenter);

public:      // data
  IndentWriter &writer;

public:      // funcs
  IndentWriterIndenter(IndentWriter &w)
    : writer(w)
  {
    writer.indent();
  }

  ~IndentWriterIndenter()
  {
    writer.dedent();
  }
};


#endif /* INDENT_WRITER_HPP */
