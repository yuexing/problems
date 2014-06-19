#ifndef TICKER_HPP
#define TICKER_HPP

// title
// header |0----------25-----------50----------75---------100|
// sizeof(header) / max_ticks = (n, g]/(prev, cur]
// spins: \|-/

/**
 * A tick sink
 * Receives ticks from some source and processes them
 **/
struct tick_sink_t {
    virtual void stop() = 0;
    // Max number of ticks; 0 = unlimited.
    virtual size_t get_max_ticks() const = 0;
    inline tick_sink_t &operator++() {
        increment(1);
        return *this;
    }
    inline tick_sink_t &operator+=(size_t count) {
        if(count) {
            increment(count);
        }
        return *this;
    }
    virtual ~tick_sink_t() {} 
    
    /**
     * Do not print any tickers at all
     **/
    static void disable_tickers();
    /**
     * If output is a tty, spin tickers, otherwise do not spin them
     **/
    static void enable_tickers();
    /**
     * Do not spin the tickers, but still prints the stars.
     * Useful when the terminal doesn't understand \b
     * default if isatty(1) is false.
     **/
    static void do_not_spin_tickers();
    /**
     * Enable tickers, with spinning.
     * default is isatty(1) is true.
     **/
    static void spin_tickers();

    enum ticker_mode_t {
        NO_TICKER,
        NO_SPIN,
        SPIN
    };

    /**
     * Report ticker state
     **/
    static ticker_mode_t get_ticker_mode();
protected:
    // Increment by a non-0 amount.
    // The public interface is operator++ and operator+=
    virtual void increment(size_t) = 0;
    static ticker_mode_t s_ticker_mode;
};

/**
 * A "ticker".
 * Prints a spinning bar (/-\|), possibly advancing with '*' or 'X' in
 * case of errors.
 **/
struct ticker_t : public tick_sink_t {
    // If title is empty or NULL, it won't be printed; otherwise it
    // will be printed with a newline.
    ticker_t(size_t max_ticks, const char *title, size_t ticks_to_spin = 1);
    // This version is for testing; uses some stream instead of "cout"
    ticker_t(
        ostream &stream,
        size_t max_ticks,
        const char * title,
        size_t ticks_to_spin,
        ticker_mode_t ticker_mode);
    ~ticker_t();
    virtual void stop();
    
    size_t get_max_ticks() const { return max_ticks; }

    // add a success tick
    void inc_succ();
    // add an error tick.
    // If there are errors, will print 'X' characters instead of '*'
    // characters.
    // The resulting output will have a number of 'X' to '*' ratio
    // proportional to the error ratio.
    void inc_err();

private:

    void writeToConsole(const char *buf, size_t count);

    void init(
        size_t max_ticks,
        const char *title,
        size_t ticks_to_spin,
        ticker_mode_t ticker_mode);

    size_t ticks; // The current tick count
    size_t spins; // The current spin count
    // The expected max number of ticks
    size_t max_ticks;
    // Number of ticks before spinning the bar
    size_t ticks_to_spin;
    // Indicates whether we have already output something (otherwise, don't output a backspace)
    bool started;
    // Number of "error" ticks.
    int errors;
    // Number of 'X' characters printed.
    int errorCharsPrinted;

    ticker_mode_t ticker_mode;

    // Stream to output to.
    ostream &out;

    void increment(size_t count);
};

#endif /* TICKER_HPP */
