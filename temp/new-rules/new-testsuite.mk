# 

ifneq ($(NEW_TESTSUITE_INCLUDED),1)

NEW_TESTSUITE_INCLUDED := 1

# Let this be the first rule, so that if we're just running 'make'
# in a test directory, slow tests will be run.
# Setting TESTSUITE_ENABLE_SLOW_TEST also causes the jwas testsuite to be included
slow-testsuite: TESTSUITE_ENABLE_SLOW_TESTS=1
slow-testsuite: testsuite
export TESTSUITE_ENABLE_SLOW_TESTS

testsuite: full-testsuite

# jwas tests are disabled by default
jwas-testsuite: TESTSUITE_ENABLE_JWAS_TESTS=1
jwas-testsuite: testsuite
export TESTSUITE_ENABLE_JWAS_TESTS

# We want the platform flags, for "NO_JAVA"
# Including all-rules.mk isn't that slow.
include $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))all-rules.mk

# Undef some debugging flags that can change the test output.
include $(RULES)/unexport-debug-flags.mk

# SGM: 2007-04-05: I moved the test for coverity_config.xml to
# Makefile, testsuite-prereq, since it's really annoying to have to
# create and remove that config all the time to test things, and the
# presence of that file is only a problem for the testsuite.

test_help::
	@echo
	@echo "-- Environment variables controlling test harness --"

include $(RULES)/testsuite-dir.mk

test_help::
	@echo "  MAX_FAILURES"
	@echo "      The maximum number of tests which can fail before giving up."
	@echo "      Default: 10"
	@echo "      beware of interaction with grouping???"

MAX_FAILURES?=10

ifeq (1,$(BUILD_FE_SUBSET))
export TEST_SUBSET=_offshore
else
export TEST_SUBSET=_all
endif

TEST_SPEC?=test_spec.xml
TEST_SPEC_FLEXNET?=test_spec_flexnet.xml

TEST_LOG?=test_log.$(mc_platform).xml
TEST_FLEX_LOG?=test_flex_log.$(mc_platform).xml

QUIET?=--quiet
test_help::
	@echo "  QUIET"
	@echo "      Specifies verbosity of the testsuite."
	@echo "      Using the empty string 'QUIET=' yields more output."
	@echo "      'QUIET=--quiet --use-ticker' yields less (see also bug 60132)."
	@echo "      Default: --quiet"

ifneq ($(RUN_DISABLED),)
ifneq ($(RUN_DISABLED),0)
ifneq ($(RUN_DISABLED),false)
override RUN_DISABLED=--run-disabled
endif
endif
endif

# Why do we have separate variables for individual things set and used
# locally, instead of appending to a variable?  This is starting to get out of
# hand.
test_help::
	@echo "  THREADS"
	@echo "      Specifies the number of tests to run in parallel."
	@echo "      Default: # of detected cores (Windows/Linux), 1 otherwise."
	@# XXX: which tests are parallel?
	@echo "      beware that many analysis tests are intrinsically parallel???"

ifneq ($(THREADS),)
THREADS_ARG:=-j $(THREADS)
else
THREADS_ARG:=
endif

ifeq ($(BUILD_FE_CLANG),1)
NO_CLANG_TESTS_ARG:=
else
NO_CLANG_TESTS_ARG:=--no-clang-tests
endif

ifneq ($(NO_JAVA),)
NO_JAVA_TESTS_ARG:=--no-java-tests
else
NO_JAVA_TESTS_ARG:=
endif

ifneq ($(NO_CSHARP),)
NO_CSHARP_TESTS_ARG:=--no-cs-tests
else
NO_CSHARP_TESTS_ARG:=
endif

ifneq ($(TA_RAW_COVERAGE_DIR),)
TA_RAW_COVERAGE_DIR_ARG:=--ta-raw-coverage-dir $(TA_RAW_COVERAGE_DIR)
else
TA_RAW_COVERAGE_DIR_ARG:=
endif

# tested only on linux: setting this affects the build-checker script, coaxing
# it to link instrumented libraries.
ifeq ($(BUILD_TYPE),COVERAGE)
  export EXTRA_ARGS = -fprofile-arcs -ftest-coverage \
                      -L$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/lib64 \
                      -L$(top_srcdir)/$(PLATFORM_PACKAGES_DIR)/lib
endif


# smcpeak 2012-02-17: When the testsuite runs under STS, it works by
# copying the entire built tree from one machine to another.  On OS/X,
# we currently build on 10.7 but test on 10.6 and 10.7.  Unfortunately,
# the 'nm' command on 10.6 cannot read executables created on 10.7,
# which means that tests that try to decode stack traces do not work.
#
# Therefore, we skip such tests when running the testsuite under STS
# on OS/X.
#
# It is not particularly easy to determine the exact set of tests that
# want to invoke 'nm', but they're all in test-checkers/crashing, so
# I have created the --no-crashing-tests switch to skip all tests in
# that directory.
ifeq ($(mc_platform)__$(TESTSUITE_FOR_STS),macosx__1)
NO_CRASHING_TESTS_ARG := --no-crashing-tests
else
NO_CRASHING_TESTS_ARG :=
endif

# TEST_FILTER is an optional regex on test names.
ifneq ($(TEST_FILTER),)
TEST_FILTER_ARG := --filter $(TEST_FILTER)
endif

COMMON_RUNTESTSUITE_OPTS:=\
	  $(THREADS_ARG) \
	  $(QUIET) \
	  $(RUN_DISABLED) \
	  $(NO_CLANG_TESTS_ARG) \
	  $(NO_JAVA_TESTS_ARG) \
	  $(NO_CSHARP_TESTS_ARG) \
	  $(NO_CRASHING_TESTS_ARG) \
	  $(TA_RAW_COVERAGE_DIR_ARG) \
	  $(TEST_FILTER_ARG)

# Unset "MAKEFLAGS"; it might pass down "TESTSUITE_DIR" which we don't want when running multiple tests.
RUN_TESTSUITE:=\
	MAKEFLAGS= \
	$(top_srcdir)/$(TEST_BIN_DIR)/run-testsuite \
	  -c $(native_top_srcdir)/$(GCC_CONFIG) \
	  $(TESTSUITE_DIR_OPT)
test_help::
	@echo
	@echo "-- Make targets for running testsuite --"

test_help::
	@echo "  testsuite"
	@echo "      Run all tests."
	@echo "  continue-testsuite"
	@echo "      Re-run only those tests that failed previously."

continue-testsuite: continue-oldlic-testsuite 

continue-oldlic-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  $(COMMON_RUNTESTSUITE_OPTS) \
	  --log $(TEST_LOG) \
	  --skip $(TEST_LOG) \
	  --append \
	  -m $(MAX_FAILURES) \
	  $(TEST_SPEC)

continue-flexnet-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	FLEXNET_TESTSUITE=1 \
	$(RUN_TESTSUITE) \
	  $(COMMON_RUNTESTSUITE_OPTS) \
	  --log $(TEST_FLEX_LOG) \
	  --skip $(TEST_FLEX_LOG) \
	  --append \
	  -m $(MAX_FAILURES) \
	  $(TEST_SPEC_FLEXNET)

full-testsuite: full-oldlic-testsuite 

full-oldlic-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  $(COMMON_RUNTESTSUITE_OPTS) \
	  --log $(TEST_LOG) \
	  -m $(MAX_FAILURES) \
	  $(TEST_SPEC)

test_help::
	@echo "  ungrouped-testsuite"
	@echo "      Run all tests indvidually."
	@echo "      (This shouldn't even exist---see bug 54446.)"

ungrouped-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  $(COMMON_RUNTESTSUITE_OPTS) \
	  --log $(TEST_LOG) \
	  -m $(MAX_FAILURES) \
          --no-grouping \
	  $(TEST_SPEC)

# Testsuite target for the pop coverage-analysis
# Includes the slow testsuite
# Setting TESTSUITE_ENABLE_SLOW_TEST also causes the jwas testsuite to be included
pop-testsuite: TESTSUITE_ENABLE_SLOW_TESTS=1
pop-testsuite: MAX_FAILURES=-1
pop-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	 $(RUN_TESTSUITE) \
	 $(COMMON_RUNTESTSUITE_OPTS) \
	 --log $(TEST_LOG) \
	 -m $(MAX_FAILURES) \
	 --no-grouping \
	 $(TEST_SPEC)

full-flexnet-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	FLEXNET_TESTSUITE=1 \
	$(RUN_TESTSUITE) \
	  $(COMMON_RUNTESTSUITE_OPTS) \
	  --log $(TEST_FLEX_LOG) \
	  -m $(MAX_FAILURES) \
	  $(TEST_SPEC_FLEXNET)

replay-testsuite:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  $(COMMON_RUNTESTSUITE_OPTS) \
	  --replay-failures $(TEST_LOG) \
	  -m $(MAX_FAILURES) \
	  --log replay-$(TEST_LOG)

test_help::
	@echo "  commit"
	@echo "      (For analysis tests) Update 'standard' event annotations."
	@echo "      The results should be manually inspected to ensure that"
	@echo "      analysis behavior matches expectations."

commit:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  --generate-expected \
	  --log commit-$(TEST_LOG) \
	  $(TEST_SPEC)

commit-failures:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  --skip $(TEST_LOG) \
	  --generate-expected \
	  --log commit-$(TEST_LOG) \
	  $(TEST_SPEC)

test_help::
	@echo "  defect-commit"
	@echo "      (For analysis tests) Inserts //#??defect annotations, which"
	@echo "      should be manually edited to indicate 'defect' or 'fpdefect'."

defect-commit:
	TEST_SUBSET=$(TEST_SUBSET) \
	$(RUN_TESTSUITE) \
	  --generate-expected \
	  --defect-annotations \
	  --log commit-$(TEST_LOG) \
	  $(TEST_SPEC)

test_help::
	@echo
	@echo "Example:"
	@echo "  MAX_FAILURES=0 make testsuite"
	@echo "runs all tests and stops at the first failure."

endif # NEW_TESTSUITE_INCLUDED

# EOF
