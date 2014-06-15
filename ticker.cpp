void ticker_unit_tests()
{
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/20, "Hello World", /*ticks_to_spin*/3,
                        tick_sink_t::SPIN);
        for(int i = 0; i < 20; ++i) {
            ++ticker;
        }
        ticker.stop();
        checkTickerOutput(ostr.str());
        assert_same_string(
            ostr.str(),
            stringb(
                "Hello World\n"
                << header
                << "*\b**|\b***|\b**|\b***|\b***|\b**|\b***|\b**|\b***|\b***|\b**|\b***|\b**|\b***|\b***|\b**|\b***|\b**|\b***|\b***\n"));
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/3,
                        tick_sink_t::SPIN);
        for(int i = 0; i < 200; ++i) {
            ++ticker;
        }
        ticker.stop();
        checkTickerOutput(ostr.str());
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/3,
                        tick_sink_t::SPIN);
        // This should work even if not spinning enough.
        for(int i = 0; i < 150; ++i) {
            ++ticker;
        }
        ticker.stop();
        checkTickerOutput(ostr.str());
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/3,
                        tick_sink_t::SPIN);
        // Values add up to 20.
        char values[] = {
            11,
            33,
            5,
            112,
            37,
            1,
            1
        };
        for(int i = 0; i < ARRAY_SIZE(values); ++i) {
            ticker+= values[i];
        }
        ticker.stop();
        checkTickerOutput(ostr.str());
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/2,
                        tick_sink_t::SPIN);

        scaled_ticker_t scal1(
            ticker,
            /*total*/30,
            /*scaled*/8);
        cond_assert(scal1.get_max_ticks() == 30);
        for(int i = 0; i < 15; ++i) {
            scal1 += 2;
        }
        // There's no need to "stop" if you ticked "total" time.
        // Should work even if "total" is 0.
        scaled_ticker_t scal2(
            ticker,
            /*total*/0,
            /*scaled*/12);
        scaled_ticker_t scal3(
            ticker,
            /*total*/7,
            /*scaled*/180);
        // Intentionally not ticking as many times, to test "stop"
        for(int i = 0; i < 5; ++i) {
            ++scal3;
        }
        scal3.stop();

        ticker.stop();
        checkTickerOutput(ostr.str());
    }
    // Test error 'X'
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/1,
                        tick_sink_t::SPIN);
        int errors = 0;
        for(int i = 0; i < 100; ++i) {
            if(i % 37 == 36) {
                ticker.inc_err();
                ticker.inc_err();
                errors += 2;
            } else {
                ticker += 2;
            }
        }
        ticker.stop();
        checkTickerOutput(ostr.str(),
                          errors,
                          200,
                          "******************X*******************X*************");
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/20, "Hello World", /*ticks_to_spin*/1,
                        tick_sink_t::SPIN);
        int errors = 0;
        for(int i = 0; i < 20; ++i) {
            if(i % 7 == 0) {
                ticker.inc_err();
                ++errors;
            } else {
                ++ticker;
            }
        }
        ticker.stop();
        checkTickerOutput(ostr.str(),
                          errors,
                          20,
                          "XXX***************XXX***************XX**************");
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/1,
                        tick_sink_t::SPIN);
        // This checks that we don't get the last 'X' wrong.
        ticker += 199;
        ticker.inc_err();
        ticker.stop();
        checkTickerOutput(ostr.str(),
                          1,
                          200,
                          "***************************************************X");
    }
    {
        // Make sure that not spinning doesn't change anything.
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/20, "Hello World", /*ticks_to_spin*/1,
                        tick_sink_t::NO_SPIN);
        int errors = 0;
        for(int i = 0; i < 20; ++i) {
            if(i % 7 == 0) {
                // This shows that an error is only represented by 1 'X'
                ticker.inc_err();
                ++errors;
            } else {
                ++ticker;
            }
        }
        ticker.stop();
        checkTickerOutput(ostr.str(),
                          errors,
                          20,
                          "XXX***************XXX***************XX**************");
    }
    {
        ostringstream ostr;
        ticker_t ticker(ostr, /*max*/200, "Hello World", /*ticks_to_spin*/1,
                        tick_sink_t::NO_SPIN);
        // This checks that we don't get the last 'X' wrong.
        ticker += 199;
        ticker.inc_err();
        ticker.stop();
        checkTickerOutput(ostr.str(),
                          /*errors*/1,
                          200,
                          "***************************************************X");
    }
    {
        null_ticker_t n;
        tick_sink_t &t = n;
        cond_assert(t.get_max_ticks() == 0);
        // This does nothing. I can't really check that it does
        // nothing, the best to check is that it doesn't crash, I guess.
        ++t;
        t += 10;
        t.stop();
    }
    tick_sink_t::disable_tickers();
    cond_assert(tick_sink_t::get_ticker_mode() == tick_sink_t::NO_TICKER);
    tick_sink_t::enable_tickers();
    cond_assert(tick_sink_t::get_ticker_mode() == default_ticker_mode());
    tick_sink_t::do_not_spin_tickers();
    cond_assert(tick_sink_t::get_ticker_mode() == tick_sink_t::NO_SPIN);
    tick_sink_t::spin_tickers();
    cond_assert(tick_sink_t::get_ticker_mode() == tick_sink_t::SPIN);
    // Try disable_tickers() again, making sure that the first test
    // doesn't just test the default.
    tick_sink_t::disable_tickers();
    cond_assert(tick_sink_t::get_ticker_mode() == tick_sink_t::NO_TICKER);
}
