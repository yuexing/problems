#ifndef TICKER_HPP
#define TICKER_HPP

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

// A tick_sink_t that does nothing
struct null_ticker_t: public tick_sink_t {
    void increment(size_t count);
    void stop();
    size_t get_max_ticks() const;
};

// A tick_sink_t that sends a given number of ticks to another tick
// sink when it is itself ticked a given other number.
struct scaled_ticker_t: public tick_sink_t
{
    // If you tick this "count" time, it will tick "underlying"
    // scaledCount times.
    scaled_ticker_t(
        tick_sink_t &underlying,
        size_t count,
        size_t scaledCount);
    void increment(size_t count);
    // Stop will jump to the scaled count if necessary; it will not
    // stop the underlying ticker.
    void stop();

    size_t get_max_ticks() const;

    ~scaled_ticker_t();

private:
    tick_sink_t &underlying;
    size_t const total;
    size_t const scaledTotal;
    size_t cur;
    size_t curScaled;
};

/**
 * Ticker factory. Allows dynamic dispatch of ticker creation.
 * Base class creates a regular ticker_t.
 **/
struct ticker_factory_t {
    // Create a ticker with the given maximum number of ticks and
    // given title.
    // The title is a string which, if non-null and non-empty, is
    // printed, followed by a newline, before the ticker itself.
    // "ticks_to_spin" indicates the number of ticks before spinning
    // the bar; it ticks are very fast this may need to be more than 1.
    virtual tick_sink_t *make_ticker(
        size_t max_ticks,
        const char */*nullable*/title,
        size_t ticks_to_spin = 1);
    virtual ~ticker_factory_t(){}
};

// A factory that creates "ticker_t" objects.
extern ticker_factory_t ticker_t_factory;

// This has to be here to avoid a mutual recursion between "system"
// and "text". I could separate this out completely to break the cycle
// but that'd be overkill.
// cov-help uses this
bool get_out_is_tty();

void ticker_unit_tests();


#endif /* TICKER_HPP */
