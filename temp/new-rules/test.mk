# 

# test.mk
# Helper to write tests using Makefiles + run-testsuite
# Attempts to minimize parsing when run from within run-testsuite
# e.g. avoid running config.guess
ifeq (,$(TEST_MK_INCLUDED))
TEST_MK_INCLUDED:=1

# The default target is the first one in the file.
# Create it, with an unlikely name, make it depend on the real default.
first-test-target: default-test-target

RULES:=$(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

# Undef some debugging flags that can change the test output.
include $(RULES)/unexport-debug-flags.mk

ifneq ($(TEST_NO_JAVA),)
# Means we're in a test directory that's not named to include Java.
ifeq ($(JAVA_IF_AVAILABLE),)
# Test writers can set JAVA_IF_AVAILABLE=1 before including test.mk
# in order to run Java if it's available on that platform.  The test
# can check NO_JAVA to find out if Java is available.
NO_JAVA:=1
endif
endif

ifneq ($(TEST_NO_CSHARP),)
# Means we're in a test directory that's not named to include C#.
ifeq ($(CSHARP_IF_AVAILABLE),)
# Test writers can set CSHARP_IF_AVAILABLE=1 before including test.mk
# in order to run C# if it's available on that platform.  The test
# can check NO_CSHARP to find out if C# is available.
NO_CSHARP:=1
endif
endif

.PHONY: new-testsuite testsuite

ifneq (,$(STANDALONE))

# Standalone: This means we don't want to run "run-testsuite", but we
# still need the set-up (in particular the testsuite dir)
# If RUN_TESTSUITE is also set, this is a recursive invocation, and we
# don't want to do the set-up again.
ifeq (,$(RUN_TESTSUITE))

include $(RULES)/paths.mk
RUN_TESTSUITE:=1

test_help::
	@echo "TESTSUITEDIR=<directory to run tests in>"

# Allow setting the testsuite dir with either TESTSUITEDIR or TESTSUITE_DIR
ifneq (,$(TESTSUITEDIR))
TESTSUITE_DIR:=$(TESTSUITEDIR)
endif
ifeq (,$(TESTSUITE_DIR))
TESTSUITE_DIR:=$(call SHELL_TO_NATIVE,$(shell mkdir -p /tmp/cov-$(USER);mktemp -d /tmp/cov-$(USER)/standalone-test-XXXXXX))
default-test-target: testsuite rm-testsuite-dir

else
default-test-target: testsuite
endif

PLATFORM:=$(mc_platform)
PREVENT_SRC_ROOT:=$(top_srcdir)
PREVENT_BIN_DIR:=$(top_srcdir)/$(RELEASE_BIN_DIR)

# Export the variables, in case of recursive $(MAKE) invocation.
export RUN_TESTSUITE
export PLATFORM
export PREVENT_SRC_ROOT
export PREVENT_BIN_DIR
export TESTSUITE_DIR
# for analysis/checkers/models/check-shipped-models
# (the only STANDALONE test, as far as I can tell)
export OBJ_DIR

rm-testsuite-dir:
	rm -r $(TESTSUITE_DIR)

endif # !RUN_TESTSUITE
endif # STANDALONE

ifeq (,$(RUN_TESTSUITE))
# Means we're not within run-testsuite
# We should run run-testsuite instead of the actual test
include $(RULES)/paths.mk

include $(RULES)/testsuite-dir.mk

default-test-target: testsuite-under-run-testsuite

# If you're not within run-testsuite, recursively run from run-testsuite
# The Makefile will have a "testsuite:" target.
# We'd like to replace it in that case, but I don't think it's possible.
# Instead, use a different target.
testsuite-under-run-testsuite:
	$(top_srcdir)/$(TEST_BIN_DIR)/run-testsuite $(TESTSUITE_DIR_OPT) 'shell:make testsuite'

commit:
	$(top_srcdir)/$(TEST_BIN_DIR)/run-testsuite $(TESTSUITE_DIR_OPT) \
	--generate-expected 'shell:make testsuite'

run-testsuite-target:
	$(top_srcdir)/$(TEST_BIN_DIR)/run-testsuite $(TESTSUITE_DIR_OPT) 'shell:make $(TARGET)'

.PHONY:complain

complain:
	@echo "Must run make testsuite from within run-testsuite"
	@echo "Use \"make\" with no arguments instead"
	@false

testsuite: complain

else # RUN_TESTSUITE
# Within run-testsuite only
# The makefile should still parse without those, however the actions
# don't need to actually work

# PLATFORM is set by run-testsuite
# It can only set variables with names in ALL CAPS on windows so it's
# not directly setting mc_platform
mc_platform:=$(PLATFORM)

include $(PREVENT_SRC_ROOT)/new-rules/config-common.mk

# Allow running Java
JPATH_DIR := $(PREVENT_SRC_ROOT)/utilities/jpath
CLASSES_DIR:=$(PREVENT_ROOT_DIR)/../classes

COV_BIN_DIR:=$(PREVENT_BIN_DIR)
TEST_BIN_DIR:=$(PREVENT_BIN_DIR)/../test-bin
UNIT_TEST_BIN_DIR:=$(PREVENT_ROOT_DIR)/unit-test-bin
FAKE_COMPILERS_BIN_DIR:=$(TEST_BIN_DIR)/fake-compilers
GCC_CONFIG:=$(PREVENT_BIN_DIR)/../gcc-config/coverity_config.xml
TEST_CL:=$(FAKE_COMPILERS_BIN_DIR)/cl

PLATFORM_PACKAGES_DIR:=$(PREVENT_SRC_ROOT)/$(mc_platform)-packages

ifneq (,$(wildcard $(PREVENT_BIN_DIR)/../config/coverity_config.xml))
$(error You should not have a coverity_config.xml in $(PREVENT_BIN_DIR)/../config/coverity_config.xml . Please remove it)
endif

# "Shell" (e.g. cygwin) version of testsuite dir, necessary to write
# make targets without a ':' in them
SHELL_TESTSUITE_DIR:=$(call NATIVE_TO_SHELL,$(TESTSUITE_DIR))
export SHELL_TESTSUITE_DIR

default-test-target: testsuite

new-testsuite:
	echo "Cannot run new-testsuite from within run-testsuite"
	exit 1

endif # RUN_TESTSUITE

# Now define some macros common to both paths.mk and test.mk
include $(RULES)/paths-common.mk

.PHONY: jsonfilter-current-output jsontest-current-output
.PHONY: jsonfilter-raw-output jsontest-raw-output
.PHONY: process-raw-output
.PHONY: compare-output compare-output2 compare-output-exactly

RAW_OUTPUT:=$(TESTSUITE_DIR)/RawOutput
CURRENT_OUTPUT:=$(TESTSUITE_DIR)/CurrentOutput

# umm, this is quickly getting unmaintainable...
SHELL_CURRENT_OUTPUT:=$(SHELL_TESTSUITE_DIR)/CurrentOutput

# Java tools for use in tests
# sgoldsmith 2011-09-19: Why is the following logic duplicated in test.mk and paths.mk?

JDK_BIN_DIR:=$(PLATFORM_PACKAGES_DIR)/jdk/bin/
JAVA_BIN_DIR:=$(PLATFORM_PACKAGES_DIR)/jdk/jre/bin/

REAL_JAVA=$(JAVA_BIN_DIR)java$(EXE_POSTFIX)
REAL_JAVAC=$(JDK_BIN_DIR)javac$(EXE_POSTFIX)
REAL_JAR=$(JDK_BIN_DIR)jar$(EXE_POSTFIX)
REAL_JAVAP=$(JDK_BIN_DIR)javap$(EXE_POSTFIX)

# The JVM will attempt to allocate memory beyond ulimit settings and the
# maximum heap size specified for the JVM (-Xmx) depending on the selected
# garbage collector.  In order to avoid insufficient memory errors,
# -XX:+UseSerialGC is specified and the JVM heap size capped.  See bug 38333.
# Note that run-testsuite lowers the virtual memory ulimit by calling
# limit_process_data_size() in utilities/run-testsuite/run-testsuite.cpp.
JAVA=$(REAL_JAVA) -Xmx256m -XX:+UseSerialGC
JAVAC=$(REAL_JAVAC) -J-Xmx256m -J-XX:+UseSerialGC
JAR=$(REAL_JAR) -J-Xmx256m -J-XX:+UseSerialGC
JAVAP=$(REAL_JAVAP) -J-Xmx256m -J-XX:+UseSerialGC
ANT_OPTS=-Xmx256m -XX:+UseSerialGC
export ANT_OPTS

CLANG_CC=$(PLATFORM_PACKAGES_DIR)/bin/clang
CLANG_CXX=$(PLATFORM_PACKAGES_DIR)/bin/clang++

JSONFILTER_RESPONSE_FILE:=jsonfilter.options
JSONFILTER_JAVA_OPTS?=
JSONFILTER=$(JAVA) -classpath $(PREVENT_ROOT_DIR)/../classes $(JSONFILTER_JAVA_OPTS) \
           com.coverity.json.JSONFilter

jsonfilter-current-output:
	mv $(CURRENT_OUTPUT) $(CURRENT_OUTPUT).bak
	$(JSONFILTER) $(CURRENT_OUTPUT).bak @$(JSONFILTER_RESPONSE_FILE) > $(CURRENT_OUTPUT)
	rm -f $(CURRENT_OUTPUT).bak

jsonfilter-raw-output:
	mv $(RAW_OUTPUT) $(RAW_OUTPUT).bak
	$(JSONFILTER) $(RAW_OUTPUT).bak @$(JSONFILTER_RESPONSE_FILE) > $(RAW_OUTPUT)
	rm -f $(RAW_OUTPUT).bak

JSONTEST_RESPONSE_FILE:=jsontest.options
JSONTEST_JAVA_OPTS?=
JSONTEST=$(JAVA) -classpath $(PREVENT_ROOT_DIR)/../classes $(JSONTEST_JAVA_OPTS) \
         com.coverity.json.JSONTest

jsontest-current-output:
	mv $(CURRENT_OUTPUT) $(CURRENT_OUTPUT).bak
	$(JSONTEST) $(CURRENT_OUTPUT).bak @$(JSONTEST_RESPONSE_FILE) > $(CURRENT_OUTPUT)
	rm -f $(CURRENT_OUTPUT).bak

jsontest-raw-output:
	mv $(RAW_OUTPUT) $(RAW_OUTPUT).bak
	$(JSONTEST) $(RAW_OUTPUT).bak @$(JSONTEST_RESPONSE_FILE) > $(RAW_OUTPUT)
	rm -f $(RAW_OUTPUT).bak

CHECK_EOL:=$(TEST_BIN_DIR)/check-eol

# what is the expected line terminator mode?
ifneq ($(filter mingw mingw64,$(mc_platform)),)
  EOLMODE := dos
else
  EOLMODE := unix
endif

COV_PROCESS_OUTPUT:=$(TEST_BIN_DIR)/cov-process-output

PROCESS=$(COV_PROCESS_OUTPUT)\
 --replace-cur-dir \
 $(EXTRA_PRE_REGEX) \
 --replace-path '$(TESTSUITE_DIR)%$$prevent$$' \
 --replace-path '$(PREVENT_ROOT_DIR)%$$prevent_root$$' \
 --replace-regex '$(mc_platform)/$$platform$$' \
 $(EXTRA_REGEX) \

process-raw-output:
	$(PROCESS) \
 -of $(CURRENT_OUTPUT) $(RAW_OUTPUT)

# Usage:
# $(call PROCESS_AND_DIFF,$(expected),$(current)[,$(process-opts)][,$(diff-opts)])
define PROCESS_AND_DIFF
$(PROCESS) $(value 3) -of $(value 2).processed $(value 2)
$(call DIFF_EXACT,$(value 1),$(value 2).processed,-w $(value 4))
endef

process-build-raw-output:
	$(PROCESS) \
        --strip-regex 'Compile time' \
        --strip-regex 'Build time' \
        --strip-regex 'Total time' \
 -of $(CURRENT_OUTPUT) $(RAW_OUTPUT)

test_help::
	@echo "GENERATED_EXPECTED_OUTPUT=1 # to regenerate EXPECTED_OUTPUT"

ifeq ($(GENERATE_EXPECTED_OUTPUT),)
DIFF_EXACT=diff -u $(3) $(1) $(2)
DIFF=$(call DIFF_EXACT,$(1),$(2),-w)
else
DIFF=cp $(2) $(1)
DIFF_EXACT=$(DIFF)
endif

compare-output:
	$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT))

# On Windows, file names may be lowercased
# Allow a mode where we don't check case on Windows.
ifneq ($(IS_WINDOWS),)
CASE_INSENSITIVE_DIFF_ARG:=-i
else
CASE_INSENSITIVE_DIFF_ARG:=
endif

compare-output-case-insensitive-windows:
	$(call DIFF_EXACT,ExpectedOutput,$(CURRENT_OUTPUT),-w $(CASE_INSENSITIVE_DIFF_ARG))

compare-variant-output:
	$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT)) \
	 || $(call DIFF,ExpectedOutput1,$(CURRENT_OUTPUT))

compare-variant-output2:
	$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT)) \
	|| $(call DIFF,ExpectedOutput1,$(CURRENT_OUTPUT)) \
	|| $(call DIFF,ExpectedOutput2,$(CURRENT_OUTPUT))

compare-variant-output3:
	$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT)) \
	|| $(call DIFF,ExpectedOutput1,$(CURRENT_OUTPUT)) \
	|| $(call DIFF,ExpectedOutput2,$(CURRENT_OUTPUT)) \
	|| $(call DIFF,ExpectedOutput3,$(CURRENT_OUTPUT))


# some tests generate ExpectedOutput on the fly in the working directory and delete it before the end
# of execution. But sometimes these tests fail before the cleaning up and the test directories are polluted
# by ExpectedOutput files. compare-output2 can be used to avoid this problem
compare-output2:
	diff -u -w $(TESTSUITE_DIR)/ExpectedOutput $(CURRENT_OUTPUT)

compare-output-exactly:
	$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT))

compare-output-ignore-bl:
	$(call DIFF_EXACT,ExpectedOutput,$(CURRENT_OUTPUT),-w -B)

# Compare-output with a "OS type"-specific variant (basically it means
# a ".windows" variant)
compare-ostype-output:
	if [ -f ExpectedOutput.$(OS_TYPE) ]; then \
		$(call DIFF,ExpectedOutput.$(OS_TYPE),$(CURRENT_OUTPUT)); \
	else \
		$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT)); \
	fi

compare-platform-output:
	if [ -f ExpectedOutput.$(mc_platform) ]; then \
		$(call DIFF,ExpectedOutput.$(mc_platform),$(CURRENT_OUTPUT)); \
	else \
		$(call DIFF,ExpectedOutput,$(CURRENT_OUTPUT)); \
	fi

compare-platform-output2:
	if [ -f ExpectedOutput.$(mc_platform) ]; then \
		$(call DIFF,ExpectedOutput.$(mc_platform),$(CURRENT_OUTPUT)); \
	else \
		diff -u -w $(TESTSUITE_DIR)/ExpectedOutput $(CURRENT_OUTPUT); \
	fi

ifeq ($(GENERATE_EXPECTED_OUTPUT),)
compare-sorted-output:
	$(SORT) $(CURRENT_OUTPUT) > $(TESTSUITE_DIR)/CurrentOutput.sorted
	$(SORT) ExpectedOutput > $(TESTSUITE_DIR)/ExpectedOutput.sorted
	diff -u -w $(TESTSUITE_DIR)/ExpectedOutput.sorted $(TESTSUITE_DIR)/CurrentOutput.sorted
else
compare-sorted-output:
	$(SORT) $(CURRENT_OUTPUT) > ExpectedOutput
endif


# Only run interpreter tests on platforms that are supported (little
# endian).  Make relevant test's "testsuite" target depend on
# "interp-if-supported" and call $(COV_MANAGE_EMIT) interp in the
# test's "interp" target.
# Big endian platforms are not supported: hpux-ia64 hpux-pa solaris-sparc
# va_list is only supported on Linux
.PHONY: interp-if-supported interp interp-if-supported-with-va-list
ifneq ($(filter linux-ia64 aix hpux-ia64 hpux-pa solaris-sparc,$(mc_platform)),)
interp-if-supported:
	@echo "Skipping interpreter test on unsupported platform."
else
interp-if-supported: interp
endif

ifeq ($(filter linux64 linux,$(mc_platform)),)
interp-if-supported-with-va-list:
	@echo "Skipping interpreter test on unsupported platform."
else
interp-if-supported-with-va-list: interp
endif


# Set HAS_COV_BUILD to 1 iff the platform has cov-build.
ifeq ($(filter aix,$(mc_platform)),)
  HAS_COV_BUILD := 1
else
  HAS_COV_BUILD := 0
endif


# DR 6978: When an incremental test overwrites a file, we need to
# ensure the timestamp will be new.  Ideally we would have a
# special-purpose program that would stat the original file, remember
# the time, overwrite, stat it again, and then compare; if still the
# same, wait a bit and do it again.
#
# But there are less than 10 tests with "test-incremental" in their
# name, so for now we'll just kludge it with yet another sleep.
#
# The $(OVERWRITE) command takes two arguments, the source and then
# the destination, just like 'cp'.
#
OVERWRITE := sleep 1; cp -f

# other macros, $(RM_CORE_FILES) in particular
include $(RULES)/test-macros.mk

# Variable for an intermediate directory
# Use "=", not ":=", because sometimes "TESTSUITE_DIR" is reassigned.
IDIR=$(TESTSUITE_DIR)/dir
SHELL_IDIR=$(SHELL_TESTSUITE_DIR)/dir


# See BZ 54498 for more information.
# Currently CIM rejects defect commits using the "L" symbol type
# so we do not emit this symbol type by default. In order to test
# the xrefs emitted to support hyperlinking in CIM, we enable emit
# of source file symbols via this environment variable.
export COVERITY_XREF_EMIT_SOURCE_FILE_SYMBOLS=1


# ----------------------- seq -----------------------
# Returns a sequence of numbers from $(1) to $(2)-1
# Example: $(call seq,0,4))
# Returns: "0 1 2 3 "
# This is a direct replacement for the unix command "seq",
# which doesn't exist on all our build machines.
define seq
$(shell \
  iter=$(1); \
  while [ $$iter -lt $(2) ]; do \
    echo -n "$$iter "; \
    iter=$$((iter + 1)); \
  done;
)
endef

# Work around possible stupid build machine misconfiguration. (BZ 63629).
# `hostname` is bound to a variable because calling it multiple times will
# cause sb/cov-analyze-build/* tests to complain.
HOSTNAME:=$(shell hostname)
ifneq ($(filter b-win7-01 b-win2k8x64-03,$(HOSTNAME)),)
PYTHON=/cygdrive/c/Python33/python
else
ifneq (,$(findstring b-netbsd,$(HOSTNAME)))
PYTHON=python2.7
else
ifneq ($(filter tainted,$(HOSTNAME)),) # BZ 63913
PYTHON=true ||
else
PYTHON=python
endif
endif
endif

endif # TEST_MK_INCLUDED
